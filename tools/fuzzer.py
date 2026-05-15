#!/usr/bin/env python3

# Written by Sonnet 4.6

"""
wsdd-native fuzzer
==================
Mutation-based black-box fuzzer for wsdd-native targeting both attack surfaces:
  - HTTP/TCP on port 5357  (WS-Transfer GET / SOAP)
  - UDP on port 3702       (WS-Discovery Probe / Resolve)

Usage:
    python3 fuzzer.py [--host HOST] [--uuid UUID] [--seed SEED]
                      [--rounds ROUNDS] [--timeout TIMEOUT] 
                      [--local-ip LOCAL_IP] [--verbose]

    --host     Target IP (default: main interface of this host)
    --uuid     Server UUID (default: auto-discovered via Probe)
    --seed     RNG seed for reproducibility
    --rounds   Number of fuzz iterations (default: 1000)
    --timeout  Socket timeout in seconds (default: 2)
    --local-ip Local interface IP for multicast (default: main interface of this host)
    --verbose  Print every payload sent

Strategies:
  1. Valid-but-mutated HTTP requests (bit flips, byte insertions, truncation)
  2. Malformed HTTP headers (oversized, missing, duplicated, illegal chars)
  3. Valid-but-mutated SOAP bodies over HTTP
  4. UDP WSD datagrams (valid mutated, garbage, oversized, replay)
  5. XML stress cases (deeply nested, huge attributes, entity-like strings)
  6. HTTP connection floods (many simultaneous half-open connections)
  7. Content-Length mismatch (declared vs actual body size)
  8. Keep-alive pipelining stress
  9. Slow sender (one byte at a time)
"""

import argparse
import random
import socket
import time
import threading
import traceback
import uuid
import sys
from typing import Optional

# ──────────────────────────────────────────────────────────────
# Constants matching the server source
# ──────────────────────────────────────────────────────────────
HTTP_PORT        = 5357
UDP_PORT         = 3702
MULTICAST_ADDR   = "239.255.255.250"
MAX_CONTENT_LEN  = 256 * 1024   # g_httpMaxContentLength
MAX_UDP_DATAGRAM = 32767        # g_wsdMaxDatagramLength
MAX_HEADERS_SIZE = 8192         # s_maxHeadersSize
MAX_URI_SIZE     = 2048         # s_maxUriSize
MAX_METHOD_SIZE  = 10           # s_maxMethodSize

SOAP_NS  = "http://www.w3.org/2003/05/soap-envelope"
WSA_NS   = "http://schemas.xmlsoap.org/ws/2004/08/addressing"
WSD_NS   = "http://schemas.xmlsoap.org/ws/2005/04/discovery"
WSDT_NS  = "http://schemas.xmlsoap.org/ws/2004/09/transfer"
WSDP_NS  = "http://schemas.xmlsoap.org/ws/2006/02/devprof"

# ──────────────────────────────────────────────────────────────
# Stats
# ──────────────────────────────────────────────────────────────
class Stats:
    def __init__(self):
        self.sent       = 0
        self.errors     = 0
        self.crashes    = 0
        self.start_time = time.time()
        self._lock      = threading.Lock()

    def record(self, sent=0, error=0, crash=0):
        with self._lock:
            self.sent    += sent
            self.errors  += error
            self.crashes += crash

    def report(self):
        elapsed = time.time() - self.start_time
        rate    = self.sent / elapsed if elapsed else 0
        print(f"\n{'─'*60}")
        print(f"  Fuzzing complete")
        print(f"  Payloads sent : {self.sent}")
        print(f"  Elapsed       : {elapsed:.1f}s  ({rate:.1f}/s)")
        print(f"  Errors        : {self.errors}")
        print(f"  Possible crash: {self.crashes}")
        print(f"{'─'*60}")

stats = Stats()

# ──────────────────────────────────────────────────────────────
# Mutation engine
# ──────────────────────────────────────────────────────────────
class Mutator:
    def __init__(self, rng: random.Random):
        self.rng = rng

    def mutate(self, data: bytes) -> bytes:
        if not data:
            return data
        choice = self.rng.randint(0, 9)
        data   = bytearray(data)

        if choice == 0:   # bit flip
            idx        = self.rng.randrange(len(data))
            data[idx] ^= 1 << self.rng.randint(0, 7)

        elif choice == 1: # byte substitution
            idx       = self.rng.randrange(len(data))
            data[idx] = self.rng.randint(0, 255)

        elif choice == 2: # insert random byte
            idx = self.rng.randrange(len(data) + 1)
            data.insert(idx, self.rng.randint(0, 255))

        elif choice == 3: # delete byte
            idx = self.rng.randrange(len(data))
            del data[idx]

        elif choice == 4: # duplicate chunk
            if len(data) > 1:
                s     = self.rng.randrange(len(data) - 1)
                e     = self.rng.randrange(s + 1, len(data))
                chunk = bytes(data[s:e])
                data  = data[:e] + bytearray(chunk) + data[e:]

        elif choice == 5: # truncate
            data = data[:self.rng.randint(1, len(data))]

        elif choice == 6: # insert interesting integer as byte
            interesting = [0, 1, 127, 128, 255, 0xFE, 0xFF]
            idx = self.rng.randrange(len(data) + 1)
            data.insert(idx, self.rng.choice(interesting))

        elif choice == 7: # overwrite with control characters
            start = self.rng.randrange(len(data))
            n     = self.rng.randint(1, min(8, len(data) - start))
            ctrl  = self.rng.choice([0, 10, 13, 0x1A, 0x7F, 0xFE, 0xFF])
            for i in range(n):
                data[start + i] = ctrl

        elif choice == 8: # repeat chunk
            if len(data) > 2:
                s     = self.rng.randrange(len(data) - 1)
                e     = self.rng.randrange(s + 1, min(s + 20, len(data)))
                chunk = bytes(data[s:e])
                reps  = self.rng.randint(2, 50)
                data  = data[:s] + bytearray(chunk * reps) + data[e:]

        else:             # replace segment with random bytes
            start = self.rng.randrange(len(data))
            n     = self.rng.randint(1, min(32, len(data) - start))
            for i in range(n):
                data[start + i] = self.rng.randint(0, 255)

        return bytes(data)

    def multi_mutate(self, data: bytes, n: int|None = None) -> bytes:
        if n is None:
            n = self.rng.randint(1, 4)
        for _ in range(n):
            data = self.mutate(data)
        return data


# ──────────────────────────────────────────────────────────────
# SOAP / WSD template builders
# ──────────────────────────────────────────────────────────────
def fresh_uuid() -> str:
    return str(uuid.uuid4())

def make_probe(msg_id: str|None = None) -> bytes:
    mid = msg_id or f"urn:uuid:{fresh_uuid()}"
    return (
        f'<?xml version="1.0" encoding="utf-8"?>'
        f'<soap:Envelope'
        f' xmlns:soap="{SOAP_NS}"'
        f' xmlns:wsa="{WSA_NS}"'
        f' xmlns:wsd="{WSD_NS}"'
        f' xmlns:wsdp="{WSDP_NS}">'
        f'<soap:Header>'
        f'<wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To>'
        f'<wsa:Action>{WSD_NS}/Probe</wsa:Action>'
        f'<wsa:MessageID>{mid}</wsa:MessageID>'
        f'</soap:Header>'
        f'<soap:Body>'
        f'<wsd:Probe><wsd:Types>wsdp:Device</wsd:Types></wsd:Probe>'
        f'</soap:Body>'
        f'</soap:Envelope>'
    ).encode()

def make_resolve(endpoint_id: str, msg_id: str|None = None) -> bytes:
    mid = msg_id or f"urn:uuid:{fresh_uuid()}"
    return (
        f'<?xml version="1.0" encoding="utf-8"?>'
        f'<soap:Envelope xmlns:soap="{SOAP_NS}" xmlns:wsa="{WSA_NS}" xmlns:wsd="{WSD_NS}">'
        f'<soap:Header>'
        f'<wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To>'
        f'<wsa:Action>{WSD_NS}/Resolve</wsa:Action>'
        f'<wsa:MessageID>{mid}</wsa:MessageID>'
        f'</soap:Header>'
        f'<soap:Body>'
        f'<wsd:Resolve>'
        f'<wsa:EndpointReference><wsa:Address>{endpoint_id}</wsa:Address></wsa:EndpointReference>'
        f'</wsd:Resolve>'
        f'</soap:Body>'
        f'</soap:Envelope>'
    ).encode()

def make_wsdt_get(endpoint_id: str, msg_id: str|None = None) -> bytes:
    mid = msg_id or f"urn:uuid:{fresh_uuid()}"
    return (
        f'<?xml version="1.0" encoding="utf-8"?>'
        f'<soap:Envelope xmlns:soap="{SOAP_NS}" xmlns:wsa="{WSA_NS}" xmlns:wsdt="{WSDT_NS}">'
        f'<soap:Header>'
        f'<wsa:To>{endpoint_id}</wsa:To>'
        f'<wsa:Action>{WSDT_NS}/Get</wsa:Action>'
        f'<wsa:MessageID>{mid}</wsa:MessageID>'
        f'<wsa:ReplyTo><wsa:Address>{WSA_NS}/role/anonymous</wsa:Address></wsa:ReplyTo>'
        f'</soap:Header>'
        f'<soap:Body/>'
        f'</soap:Envelope>'
    ).encode()

def make_http_request(server_uuid: str, body: bytes,
                      method: str = "POST",
                      content_type: str = "application/soap+xml",
                      content_length: int|None = None,
                      extra_headers: str = "") -> bytes:
    cl = content_length if content_length is not None else len(body)
    hdr = (
        f"{method} /{server_uuid} HTTP/1.1\r\n"
        f"Host: localhost\r\n"
        f"Content-Type: {content_type}\r\n"
        f"Content-Length: {cl}\r\n"
        f"{extra_headers}"
        f"\r\n"
    )
    return hdr.encode() + body


# ──────────────────────────────────────────────────────────────
# Network helpers
# ──────────────────────────────────────────────────────────────
def tcp_send(host: str, port: int, data: bytes, timeout: float,
             read_response: bool = True) -> Optional[bytes]:
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(timeout)
            s.connect((host, port))
            s.sendall(data)
            if read_response:
                chunks = []
                try:
                    while True:
                        chunk = s.recv(4096)
                        if not chunk:
                            break
                        chunks.append(chunk)
                except socket.timeout:
                    pass
                return b"".join(chunks)
            return b""
    except (ConnectionRefusedError, OSError):
        return None

def udp_send_unicast(host: str, port: int, data: bytes, timeout: float) -> Optional[bytes]:
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.settimeout(timeout)
            s.sendto(data, (host, port))
            try:
                resp, _ = s.recvfrom(65535)
                return resp
            except socket.timeout:
                return b""
    except OSError:
        return None

def udp_send_multicast(local_ip: str, port: int, data: bytes, timeout: float) -> Optional[bytes]:
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.bind((local_ip, 0))
            s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF,
                         socket.inet_aton(local_ip))
            s.settimeout(timeout)
            s.sendto(data, (MULTICAST_ADDR, port))
            try:
                resp, _ = s.recvfrom(65535)
                return resp
            except socket.timeout:
                return b""
    except OSError:
        return None

def is_alive(host: str, port: int, timeout: float) -> bool:
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(timeout)
            s.connect((host, port))
            return True
    except Exception:
        return False

def discover_uuid(local_ip: str, timeout: float) -> Optional[str]:
    import re
    probe = make_probe()
    resp  = udp_send_multicast(local_ip, UDP_PORT, probe, timeout * 3)
    if not resp:
        return None
    text = resp.decode(errors="replace")
    m = re.search(r"<wsa:Address>urn:uuid:([0-9a-f-]{36})</wsa:Address>", text)
    return m.group(1) if m else None


# ──────────────────────────────────────────────────────────────
# Fuzzer
# ──────────────────────────────────────────────────────────────
class Fuzzer:
    def __init__(self, host: str, local_ip: str, server_uuid: str, timeout: float,
                 rng: random.Random, verbose: bool):
        self.host        = host
        self.local_ip    = local_ip
        self.uuid        = server_uuid
        self.endpoint_id = f"urn:uuid:{server_uuid}"
        self.timeout     = timeout
        self.rng         = rng
        self.mut         = Mutator(rng)
        self.verbose     = verbose
        self.strategies  = [
            self._fuzz_http_headers,
            self._fuzz_http_body_mutation,
            self._fuzz_http_content_length_mismatch,
            self._fuzz_http_method_uri,
            self._fuzz_http_keep_alive_pipeline,
            self._fuzz_http_raw_garbage,
            self._fuzz_udp_probe_mutation,
            self._fuzz_udp_resolve_mutation,
            self._fuzz_udp_raw_garbage,
            self._fuzz_udp_oversized,
            self._fuzz_xml_stress_http,
            self._fuzz_xml_stress_udp,
            self._fuzz_connection_flood,
            self._fuzz_slow_sender,
        ]

    def _log(self, strategy: str, payload: bytes):
        if self.verbose:
            preview = payload[:120].replace(b"\r\n", b"[CRLF]").replace(b"\n", b"[LF]")
            print(f"  [{strategy}] {len(payload)}b: {preview!r}")

    def check_alive(self) -> bool:
        return is_alive(self.host, HTTP_PORT, self.timeout)

    # ── HTTP ──────────────────────────────────────────────────
    def _fuzz_http_headers(self):
        body = make_wsdt_get(self.endpoint_id)
        variants = [
            # Missing Content-Length
            f"POST /{self.uuid} HTTP/1.1\r\nHost: x\r\nContent-Type: application/soap+xml\r\n\r\n".encode() + body,
            # Missing Content-Type
            f"POST /{self.uuid} HTTP/1.1\r\nHost: x\r\nContent-Length: {len(body)}\r\n\r\n".encode() + body,
            # Content-Length exceeds cap
            f"POST /{self.uuid} HTTP/1.1\r\nHost: x\r\nContent-Type: application/soap+xml\r\nContent-Length: {MAX_CONTENT_LEN + 1}\r\n\r\n".encode() + body,
            # Negative Content-Length
            f"POST /{self.uuid} HTTP/1.1\r\nHost: x\r\nContent-Type: application/soap+xml\r\nContent-Length: -1\r\n\r\n".encode() + body,
            # Content-Length zero with body
            f"POST /{self.uuid} HTTP/1.1\r\nHost: x\r\nContent-Type: application/soap+xml\r\nContent-Length: 0\r\n\r\n".encode() + body,
            # Wrong Content-Type
            f"POST /{self.uuid} HTTP/1.1\r\nHost: x\r\nContent-Type: text/plain\r\nContent-Length: {len(body)}\r\n\r\n".encode() + body,
            # Content-Type with null-byte charset
            f"POST /{self.uuid} HTTP/1.1\r\nHost: x\r\nContent-Type: application/soap+xml; charset=\x00\x01\r\nContent-Length: {len(body)}\r\n\r\n".encode() + body,
            # Duplicate Content-Length
            f"POST /{self.uuid} HTTP/1.1\r\nHost: x\r\nContent-Type: application/soap+xml\r\nContent-Length: {len(body)}\r\nContent-Length: 0\r\n\r\n".encode() + body,
            # Oversized Host header
            f"POST /{self.uuid} HTTP/1.1\r\nHost: {'x'*8000}\r\nContent-Type: application/soap+xml\r\nContent-Length: {len(body)}\r\n\r\n".encode() + body,
            # HTTP/0.9 (below min version)
            f"POST /{self.uuid} HTTP/0.9\r\nHost: x\r\nContent-Type: application/soap+xml\r\nContent-Length: {len(body)}\r\n\r\n".encode() + body,
            # HTTP/9.9 (above max version)
            f"POST /{self.uuid} HTTP/9.9\r\nHost: x\r\nContent-Type: application/soap+xml\r\nContent-Length: {len(body)}\r\n\r\n".encode() + body,
            # Missing final CRLF
            f"POST /{self.uuid} HTTP/1.1\r\nHost: x\r\nContent-Type: application/soap+xml\r\nContent-Length: {len(body)}\r\n".encode() + body,
            # LF-only line endings
            f"POST /{self.uuid} HTTP/1.1\nHost: x\nContent-Type: application/soap+xml\nContent-Length: {len(body)}\n\n".encode() + body,
            # Content-Type with 3 semicolon-separated parts
            f"POST /{self.uuid} HTTP/1.1\r\nHost: x\r\nContent-Type: application/soap+xml; charset=utf-8; extra=bad\r\nContent-Length: {len(body)}\r\n\r\n".encode() + body,
        ]
        payload = self.rng.choice(variants)
        if self.rng.random() < 0.4:
            payload = self.mut.mutate(payload)
        self._log("http_headers", payload)
        tcp_send(self.host, HTTP_PORT, payload, self.timeout)
        stats.record(sent=1)

    def _fuzz_http_body_mutation(self):
        base_bodies = [
            make_wsdt_get(self.endpoint_id),
            make_probe(),
            make_resolve(self.endpoint_id),
        ]
        body    = self.mut.multi_mutate(self.rng.choice(base_bodies),
                                        n=self.rng.randint(1, 6))
        body    = body[:MAX_CONTENT_LEN]
        payload = make_http_request(self.uuid, body)
        self._log("http_body", payload)
        tcp_send(self.host, HTTP_PORT, payload, self.timeout)
        stats.record(sent=1)

    def _fuzz_http_content_length_mismatch(self):
        body   = make_wsdt_get(self.endpoint_id)
        choice = self.rng.randint(0, 3)
        if choice == 0:
            declared = len(body) + self.rng.randint(1, 1000)   # larger
        elif choice == 1:
            declared = max(1, len(body) - self.rng.randint(1, len(body)//2))  # smaller
        elif choice == 2:
            declared = 0
        else:
            declared = MAX_CONTENT_LEN
        payload = make_http_request(self.uuid, body, content_length=declared)
        self._log("http_cl_mismatch", payload)
        tcp_send(self.host, HTTP_PORT, payload, self.timeout)
        stats.record(sent=1)

    def _fuzz_http_method_uri(self):
        body    = make_wsdt_get(self.endpoint_id)
        methods = ["GET", "DELETE", "PUT", "OPTIONS", "HEAD",
                   "X" * MAX_METHOD_SIZE, "X" * (MAX_METHOD_SIZE + 1),
                   "PO\rST", "\x00POST", "POST\x00"]
        uris    = [
            "/",
            "/" + "A" * MAX_URI_SIZE,
            "/" + "A" * (MAX_URI_SIZE + 1),
            "/../../../etc/passwd",
            f"/{self.uuid}/../../../etc/passwd",
            "/\x00",
            f"/{self.uuid}\x00extra",
            "",
        ]
        method  = self.rng.choice(methods)
        uri     = self.rng.choice(uris)
        payload = (
            f"{method} {uri} HTTP/1.1\r\n"
            f"Host: localhost\r\n"
            f"Content-Type: application/soap+xml\r\n"
            f"Content-Length: {len(body)}\r\n"
            f"\r\n"
        ).encode() + body
        if self.rng.random() < 0.3:
            payload = self.mut.mutate(payload)
        self._log("http_method_uri", payload)
        tcp_send(self.host, HTTP_PORT, payload, self.timeout)
        stats.record(sent=1)

    def _fuzz_http_keep_alive_pipeline(self):
        body = make_wsdt_get(self.endpoint_id)
        req  = make_http_request(self.uuid, body, extra_headers="Connection: keep-alive\r\n")
        n    = self.rng.randint(2, 15)
        payload = req * n
        self._log("http_pipeline", payload)
        tcp_send(self.host, HTTP_PORT, payload, self.timeout)
        stats.record(sent=1)

    def _fuzz_http_raw_garbage(self):
        size    = self.rng.randint(1, min(MAX_CONTENT_LEN, 4096))
        payload = bytes(self.rng.randint(0, 255) for _ in range(size))
        self._log("http_garbage", payload)
        tcp_send(self.host, HTTP_PORT, payload, self.timeout)
        stats.record(sent=1)

    # ── UDP ───────────────────────────────────────────────────
    def _fuzz_udp_probe_mutation(self):
        payload = self.mut.multi_mutate(make_probe(), n=self.rng.randint(1, 5))
        payload = payload[:MAX_UDP_DATAGRAM]
        self._log("udp_probe", payload)
        udp_send_multicast(self.local_ip, UDP_PORT, payload, self.timeout)
        stats.record(sent=1)

    def _fuzz_udp_resolve_mutation(self):
        payload = self.mut.multi_mutate(make_resolve(self.endpoint_id),
                                        n=self.rng.randint(1, 5))
        payload = payload[:MAX_UDP_DATAGRAM]
        self._log("udp_resolve", payload)
        udp_send_unicast(self.host, UDP_PORT, payload, self.timeout)
        stats.record(sent=1)

    def _fuzz_udp_raw_garbage(self):
        size    = self.rng.randint(1, MAX_UDP_DATAGRAM)
        payload = bytes(self.rng.randint(0, 255) for _ in range(size))
        self._log("udp_garbage", payload)
        udp_send_unicast(self.host, UDP_PORT, payload, self.timeout)
        stats.record(sent=1)

    def _fuzz_udp_oversized(self):
        # Probe the boundary at g_wsdMaxDatagramLength = 32767
        size  = self.rng.choice([MAX_UDP_DATAGRAM - 1, MAX_UDP_DATAGRAM,
                                  MAX_UDP_DATAGRAM + 1, 65507])
        base  = make_probe()
        payload = (base * (size // len(base) + 1))[:size]
        self._log("udp_oversized", payload)
        try:
            udp_send_unicast(self.host, UDP_PORT, payload, self.timeout)
        except OSError:
            pass
        stats.record(sent=1)

    # ── XML stress ────────────────────────────────────────────
    def _fuzz_xml_stress_http(self):
        case = self.rng.randint(0, 6)

        if case == 0:
            # Deeply nested elements
            depth      = self.rng.randint(100, 2000)
            open_tags  = "".join(f"<x{i}>" for i in range(depth))
            close_tags = "".join(f"</x{i}>" for i in range(depth - 1, -1, -1))
            body       = f'<?xml version="1.0"?><r>{open_tags}X{close_tags}</r>'.encode()

        elif case == 1:
            # Huge attribute value
            attr_len = self.rng.randint(10_000, MAX_CONTENT_LEN - 500)
            body = (
                f'<?xml version="1.0"?>'
                f'<soap:Envelope xmlns:soap="{SOAP_NS}">'
                f'<soap:Header attr="{"A" * attr_len}"/>'
                f'</soap:Envelope>'
            ).encode()

        elif case == 2:
            # Many attributes
            n_attrs = self.rng.randint(100, 500)
            attrs   = " ".join(f'a{i}="v{i}"' for i in range(n_attrs))
            body    = f'<?xml version="1.0"?><soap:Envelope xmlns:soap="{SOAP_NS}" {attrs}/>'.encode()

        elif case == 3:
            # Null bytes in XML
            body = bytearray(make_wsdt_get(self.endpoint_id))
            for _ in range(self.rng.randint(1, 5)):
                idx       = self.rng.randrange(len(body))
                body[idx] = 0
            body = bytes(body)

        elif case == 4:
            # Entity-like strings without DTD
            body = (
                f'<?xml version="1.0"?>'
                f'<soap:Envelope xmlns:soap="{SOAP_NS}">'
                f'<soap:Header>&amp;xxe;&amp;bomb;{"&amp;x;" * 1000}</soap:Header>'
                f'</soap:Envelope>'
            ).encode()

        elif case == 5:
            # Mismatched namespaces
            body = (
                f'<?xml version="1.0"?>'
                f'<z:Envelope xmlns:z="wrong-ns">'
                f'<z:Header><wsa:Action xmlns:wsa="{WSA_NS}">{WSDT_NS}/Get</wsa:Action></z:Header>'
                f'<z:Body/></z:Envelope>'
            ).encode()

        else:
            # Very long namespace URI
            long_ns = "http://example.com/" + "x" * 5000
            body    = f'<?xml version="1.0"?><r xmlns="{long_ns}"/>'.encode()

        body    = body[:MAX_CONTENT_LEN]
        payload = make_http_request(self.uuid, body)
        self._log("xml_stress_http", payload)
        tcp_send(self.host, HTTP_PORT, payload, self.timeout)
        stats.record(sent=1)

    def _fuzz_xml_stress_udp(self):
        case = self.rng.randint(0, 4)

        if case == 0:
            depth      = self.rng.randint(50, 500)
            open_tags  = "".join(f"<x{i}>" for i in range(depth))
            close_tags = "".join(f"</x{i}>" for i in range(depth - 1, -1, -1))
            payload    = f'<?xml version="1.0"?><r>{open_tags}X{close_tags}</r>'.encode()
        elif case == 1:
            # Huge MessageID
            payload = make_probe(msg_id="urn:uuid:" + "x" * 5000)
        elif case == 2:
            # Truncated (malformed)
            p       = make_probe()
            payload = p[:self.rng.randint(1, len(p))]
        elif case == 3:
            payload = self.mut.multi_mutate(make_probe(), n=self.rng.randint(3, 10))
        else:
            # Replay same MessageID many times (hits the dedup window)
            mid     = f"urn:uuid:{fresh_uuid()}"
            payload = make_probe(msg_id=mid)
            for _ in range(15):   # > g_maxKnownMessages (10) to force eviction
                udp_send_multicast(self.local_ip, UDP_PORT, payload, self.timeout)
                stats.record(sent=1)
            return

        payload = payload[:MAX_UDP_DATAGRAM]
        self._log("xml_stress_udp", payload)
        udp_send_multicast(self.local_ip, UDP_PORT, payload, self.timeout)
        stats.record(sent=1)

    # ── Connection / resource ─────────────────────────────────
    def _fuzz_connection_flood(self):
        """Open many half-open connections without completing the request."""
        n       = self.rng.randint(5, 25)
        sockets = []
        try:
            for _ in range(n):
                try:
                    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    s.settimeout(self.timeout)
                    s.connect((self.host, HTTP_PORT))
                    # Send partial headers — never the terminating blank line
                    s.sendall((
                        f"POST /{self.uuid} HTTP/1.1\r\n"
                        f"Host: localhost\r\n"
                        f"Content-Type: application/soap+xml\r\n"
                    ).encode())
                    sockets.append(s)
                except OSError:
                    break
            self._log("conn_flood", f"opened {len(sockets)} half-open conns".encode())
            stats.record(sent=1)
            time.sleep(self.rng.uniform(0.05, 0.5))
        finally:
            for s in sockets:
                try:
                    s.close()
                except OSError:
                    pass

    def _fuzz_slow_sender(self):
        """Send a valid request byte-by-byte with small random delays."""
        body    = make_wsdt_get(self.endpoint_id)
        payload = make_http_request(self.uuid, body)
        self._log("slow_sender", payload)
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(self.timeout * 3)
                s.connect((self.host, HTTP_PORT))
                for byte in payload:
                    s.send(bytes([byte]))
                    time.sleep(self.rng.uniform(0, 0.015))
        except OSError:
            pass
        stats.record(sent=1)

    # ── Main loop ─────────────────────────────────────────────
    def run(self, rounds: int):
        print(f"  Target  : {self.host}:{HTTP_PORT} (HTTP) / {UDP_PORT} (UDP)")
        print(f"  UUID    : {self.uuid}")
        print(f"  Rounds  : {rounds}")
        print(f"  Timeout : {self.timeout}s")
        print(f"{'─'*60}")

        for i in range(rounds):
            strategy = self.rng.choice(self.strategies)
            try:
                strategy()
            except Exception:
                if self.verbose:
                    traceback.print_exc()
                stats.record(error=1)

            if (i + 1) % 50 == 0:
                alive = self.check_alive()
                pct   = (i + 1) / rounds * 100
                print(f"  [{pct:5.1f}%] round {i+1}/{rounds}"
                      f"  sent={stats.sent}  errors={stats.errors}  alive={alive}")
                if not alive:
                    print(f"\n  !! SERVER NOT RESPONDING after round {i+1} !!")
                    print(f"  !! Last strategy: {strategy.__name__} !!")
                    stats.record(crash=1)
                    time.sleep(2)
                    if not self.check_alive():
                        print("  !! Server did not recover — stopping !!")
                        break
                    print("  Server recovered")

def get_local_ip() -> str:
    """Get the machine's real outbound IP (not loopback)."""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(("8.8.8.8", 80))
            return s.getsockname()[0]
    except OSError as e:
        print(f"  ERROR: Cannot determine local IP address: {e}")
        print(f"  Is the network up?")
        sys.exit(1)

# ──────────────────────────────────────────────────────────────
# Entry point
# ──────────────────────────────────────────────────────────────
def main():
    local_ip = get_local_ip()
    parser = argparse.ArgumentParser(
        description="Mutation-based fuzzer for wsdd-native",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--host",    default=local_ip)
    parser.add_argument("--uuid",    default=None,
                        help="Server UUID (auto-discovered if omitted)")
    parser.add_argument("--seed",    type=int, default=None,
                        help="RNG seed for reproducibility")
    parser.add_argument("--rounds",  type=int, default=1000)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--local-ip", dest='local_ip', default=local_ip)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    seed = args.seed if args.seed is not None else random.randint(0, 2**32)
    rng  = random.Random(seed)

    print(f"{'─'*60}")
    print(f"  wsdd-native fuzzer")
    print(f"  RNG seed: {seed}  (use --seed {seed} to reproduce)")
    print(f"{'─'*60}")

    server_uuid = args.uuid
    if not server_uuid:
        print("  Discovering server UUID via UDP Probe...")
        server_uuid = discover_uuid(args.local_ip, args.timeout)
        if server_uuid:
            print(f"  Discovered UUID: {server_uuid}")
        else:
            server_uuid = str(uuid.uuid4())
            print(f"  Discovery failed; using placeholder UUID: {server_uuid}")
            print(f"  (Pass --uuid <real-uuid> for full HTTP coverage)")

    if not is_alive(args.host, HTTP_PORT, args.timeout):
        print(f"\n  ERROR: Cannot connect to {args.host}:{HTTP_PORT}")
        print(f"  Is wsdd-native running?")
        sys.exit(1)

    fuzzer = Fuzzer(
        host=args.host,
        local_ip=args.local_ip,
        server_uuid=server_uuid,
        timeout=args.timeout,
        rng=rng,
        verbose=args.verbose,
    )

    try:
        fuzzer.run(args.rounds)
    except KeyboardInterrupt:
        print("\n  Interrupted")

    stats.report()
    sys.exit(2 if stats.crashes > 0 else 0)


if __name__ == "__main__":
    main()
