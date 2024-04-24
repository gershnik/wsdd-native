// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#include "wsd_server.h"

static constexpr size_t g_maxKnownMessages = 10;

const sys_string g_soapUri = S("http://www.w3.org/2003/05/soap-envelope");
const sys_string g_wsaUri  = S("http://schemas.xmlsoap.org/ws/2004/08/addressing");
const sys_string g_wsdUri  = S("http://schemas.xmlsoap.org/ws/2005/04/discovery");
const sys_string g_wsdpUri = S("http://schemas.xmlsoap.org/ws/2006/02/devprof");
const sys_string g_pubUri  = S("http://schemas.microsoft.com/windows/pub/2005/07");
const sys_string g_wsxUri  = S("http://schemas.xmlsoap.org/ws/2004/09/mex");
const sys_string g_pnpxUri = S("http://schemas.microsoft.com/windows/pnpx/2005/10");
const sys_string g_wsdtUri = S("http://schemas.xmlsoap.org/ws/2004/09/transfer");

const sys_string g_wsdUrn  = S("urn:schemas-xmlsoap-org:ws:2005:04:discovery");

using namespace std::literals;


class WSDResponseBuilder {
private:
    struct Namespaces {
        XmlNs * soap;
        XmlNs * wsa;
        XmlNs * wsd;
        XmlNs * pub;
        XmlNs * wsx;
        XmlNs * wsdp;
        XmlNs * pnpx;
    };
public:
    struct AppSequence  {
        size_t instanceId;
        size_t messageNumber;
    };

    struct Hello {
        sys_string endpointIdentifier;
    };
    struct Bye {
        sys_string endpointIdentifier;
    };
    struct ProbeMatch {
        sys_string endpointIdentifier;
    };
    struct ResolveMatch {
        sys_string endpointIdentifier;
        ip::tcp::endpoint httpEndpoint;
        sys_string httpPath;
    };
    struct ResponseToGet {
        sys_string endpointIdentifier;
        sys_string friendlyName;
        sys_string fullComputerName;
        ip::address hostAddr;
        XmlDoc * metadataTemplate = nullptr;
    };

private:
    using BodyType = std::variant<std::monostate, Hello, Bye, ProbeMatch, ResolveMatch, ResponseToGet>;

public:
    WSDResponseBuilder() {
    }

    auto setTo(const sys_string & to) -> WSDResponseBuilder & 
        { m_to = to; return *this; }
    auto setAction(const sys_string & action) -> WSDResponseBuilder & 
        { m_action = action; return *this; }
    auto setRelatesTo(const sys_string & relatesTo) -> WSDResponseBuilder & 
        { m_relatesTo = relatesTo; return *this; }
    auto setAppSequence(AppSequence && val) -> WSDResponseBuilder & 
        { m_appSequence = std::move(val); return *this; }

    template<class T>
    auto setBody(T && val) -> WSDResponseBuilder & 
    requires(std::is_assignable_v<BodyType, decltype(std::move(val))> && 
             !std::is_same_v<std::remove_cvref_t<T>, std::monostate>)
        { m_body = std::move(val); return *this; }
    
    auto build() -> std::unique_ptr<XmlDoc> {
        if (!m_to || !m_action || std::holds_alternative<std::monostate>(m_body))
            std::terminate();

        auto doc = XmlDoc::create(u8"1.0");

        doc->setRootElement(XmlNode::create(nullptr, u8"Envelope"));
        auto envelopeNode = doc->getRootElement();
        
        Namespaces ns = {
            .soap = &envelopeNode->newNs(xml_str(g_soapUri), u8"soap"),
            .wsa  = &envelopeNode->newNs(xml_str(g_wsaUri),  u8"wsa"),
            .wsd  = &envelopeNode->newNs(xml_str(g_wsdUri),  u8"wsd"),
            .pub  = &envelopeNode->newNs(xml_str(g_pubUri),  u8"pub"),
            .wsx  = &envelopeNode->newNs(xml_str(g_wsxUri),  u8"wsx"),
            .wsdp = &envelopeNode->newNs(xml_str(g_wsdpUri), u8"wsdp"),
            .pnpx = &envelopeNode->newNs(xml_str(g_pnpxUri), u8"pnpx")
        };
        envelopeNode->setNs(ns.soap);

        auto & headerNode = envelopeNode->newChild(ns.soap, u8"Header");
        headerNode.newTextChild(ns.wsa, u8"To", xml_str(*m_to));
        headerNode.newTextChild(ns.wsa, u8"Action", xml_str(*m_action));
        headerNode.newTextChild(ns.wsa, u8"MessageID", xml_str(Uuid::generate().urn()));

        if (m_relatesTo)
            headerNode.newTextChild(ns.wsa, u8"RelatesTo", xml_str(*m_relatesTo));

        if (m_appSequence) {
            auto & appSequenceNode = headerNode.newChild(ns.wsd, u8"AppSequence");
            appSequenceNode.newAttr(nullptr, u8"InstanceId", xml_str(std::to_string(m_appSequence->instanceId)));
            appSequenceNode.newAttr(nullptr, u8"SequenceId", xml_str(Uuid::generate().urn()));
            appSequenceNode.newAttr(nullptr, u8"MessageNumber", xml_str(std::to_string(m_appSequence->messageNumber)));
        }

        auto & bodyNode = envelopeNode->newChild(ns.soap, u8"Body");

        std::visit([&] (const auto & val) { fill(val, bodyNode, ns); }, m_body);

        return doc;
    }

private:
    void addEndpointReference(const Namespaces & ns, XmlNode & node, const sys_string & address) {
        auto & endpointReference = node.newChild(ns.wsa, u8"EndpointReference");
        endpointReference.newTextChild(ns.wsa, u8"Address", xml_str(address));
    }
    void addTypes(const Namespaces & ns, XmlNode & node) {
        node.newTextChild(ns.wsd, u8"Types", u8"wsdp:Device pub:Computer");
    }
    void addMetadataVersion(const Namespaces & ns, XmlNode & node) {
        node.newTextChild(ns.wsd, u8"MetadataVersion", u8"1");
    }

    void fill(const std::monostate &, XmlNode &, const Namespaces &) {
        std::terminate();
    }
    
    void fill(const Hello & val, XmlNode & bodyNode, const Namespaces & ns) {
        auto & hello = bodyNode.newChild(ns.wsd, u8"Hello");
        addEndpointReference(ns, hello, val.endpointIdentifier);
        addMetadataVersion(ns, hello);
    }
    
    void fill(const Bye & val, XmlNode & bodyNode, const Namespaces & ns) {
        auto & bye = bodyNode.newChild(ns.wsd, u8"Bye");
        addEndpointReference(ns, bye, val.endpointIdentifier);
    }
    
    void fill(const ProbeMatch & val, XmlNode & bodyNode, const Namespaces & ns) {
        auto & probeMatches = bodyNode.newChild(ns.wsd, u8"ProbeMatches");
        auto & probeMatch = probeMatches.newChild(ns.wsd, u8"ProbeMatch");
        addEndpointReference(ns, probeMatch, val.endpointIdentifier);
        addTypes(ns, probeMatch);
        addMetadataVersion(ns, probeMatch);
    }

    void fill(const ResolveMatch & val, XmlNode & bodyNode, const Namespaces & ns) {
        auto & resolveMatches = bodyNode.newChild(ns.wsd, u8"ResolveMatches");
        auto & resolveMatch = resolveMatches.newChild(ns.wsd, u8"ResolveMatch");
        addEndpointReference(ns, resolveMatch, val.endpointIdentifier);
        addTypes(ns, resolveMatch);
        auto xaddr = makeHttpUrl(val.httpEndpoint) + S("/") + val.httpPath;
        resolveMatch.newTextChild(ns.wsd, u8"XAddrs", xml_str(xaddr));
        addMetadataVersion(ns, resolveMatch);
    }

    void fill(const ResponseToGet & val, XmlNode & bodyNode, const Namespaces & ns) {
        
        if (val.metadataTemplate) {
            auto newNode = bodyNode.document()->copyNode(*val.metadataTemplate->getRootElement());
            
            replacePlaceholders(*newNode, val);
            
            bodyNode.addChild(*newNode);
            newNode.release();
            
            xmlReconciliateNs(c_ptr(bodyNode.document()), c_ptr(bodyNode.document()->getRootElement()));
            
        } else {
            
            auto & metadata = bodyNode.newChild(ns.wsx, u8"Metadata");
            {
                auto & section = metadata.newChild(ns.wsx, u8"MetadataSection");
                section.newAttr(nullptr, u8"Dialect", xml_str(g_wsdpUri + S("/ThisDevice")));
                auto & device = section.newChild(ns.wsdp, u8"ThisDevice");
                device.newTextChild(ns.wsdp, u8"FriendlyName",    xml_str(val.friendlyName));
                device.newTextChild(ns.wsdp, u8"FirmwareVersion", u8"1.0");
                device.newTextChild(ns.wsdp, u8"SerialNumber",    u8"1");
            }
            {
                auto & section = metadata.newChild(ns.wsx, u8"MetadataSection");
                section.newAttr(nullptr, u8"Dialect", xml_str(g_wsdpUri + S("/ThisModel")));
                auto & model = section.newChild(ns.wsdp, u8"ThisModel");
                model.newTextChild(ns.wsdp, u8"Manufacturer",   u8"wsddn"); //FIXME!
                model.newTextChild(ns.wsdp, u8"ModelName",      u8"wsddn");
                model.newTextChild(ns.pnpx, u8"DeviceCategory", u8"Computers");
            }
            {
                auto & section = metadata.newChild(ns.wsx, u8"MetadataSection");
                section.newAttr(nullptr, u8"Dialect", xml_str(g_wsdpUri + S("/Relationship")));
                auto & relationship = section.newChild(ns.wsdp, u8"Relationship");
                relationship.newAttr(nullptr, u8"Type",  xml_str(g_wsdpUri + S("/host")));
                auto & host = relationship.newChild(ns.wsdp, u8"Host");
                addEndpointReference(ns, host, val.endpointIdentifier);
                host.newTextChild(ns.wsdp, u8"Types", u8"pub:Computer");
                host.newTextChild(ns.wsdp, u8"ServiceId", xml_str(val.endpointIdentifier));
                host.newTextChild(ns.pub, u8"Computer", xml_str(val.fullComputerName));
            }
        }
    }
    
private:
    void replacePlaceholders(XmlNode & node, const ResponseToGet & data) {
        replacePlaceholdersInSelf(node, data);
        if (auto child = node.firstChild())
            replacePlaceholdersInSelfSiblingsAndChildren(*child, data);
    }
    
    std::u8string replaceInString(sys_string::char_access & str, const ResponseToGet & data) {
        std::u8string ret;
        ret.reserve(str.size());
        
        auto dest = std::back_inserter(ret);
        auto first = (const char8_t *)str.data();
        auto last = first + str.size();
        bool inDollar = false;
        while (first != last) {
            char8_t c = *first;
            if (inDollar) {
                inDollar = false;
                if (c == '$') {
                    *dest++ = c;
                } else {
                    auto rest = std::u8string_view(first, last - first);
                    if (auto test = u8"ENDPOINT_ID"sv; rest.starts_with(test)) {
                        sys_string::char_access access(data.endpointIdentifier);
                        dest = std::copy(access.data(), access.data() + access.size(), dest);
                        first += test.size();
                        continue;
                    }
                    if (auto test = u8"SMB_HOST_DESCRIPTION"sv; rest.starts_with(test)) {
                        sys_string::char_access access(data.friendlyName);
                        dest = std::copy(access.data(), access.data() + access.size(), dest);
                        first += test.size();
                        continue;
                    }
                    if (auto test = u8"SMB_FULL_HOST_NAME"sv; rest.starts_with(test)) {
                        sys_string::char_access access(data.fullComputerName);
                        dest = std::copy(access.data(), access.data() + access.size(), dest);
                        first += test.size();
                        continue;
                    }
                    if (auto test = u8"IP_ADDR"sv; rest.starts_with(test)) {
                        auto str = data.hostAddr.to_string();
                        dest = std::copy(str.data(), str.data() + str.size(), dest);
                        first += test.size();
                        continue;
                    }
                }
            } else {
                if (c == '$') {
                    inDollar = true;
                } else {
                    *dest++ = c;
                }
            }
            ++first;
        }
        return ret;
    }
    
    void replacePlaceholdersInSelf(XmlNode & node, const ResponseToGet & data) {
        if (node.type() == XML_TEXT_NODE) {
            auto cont = node.getContent();
            
            sys_string::char_access access(cont);
            if (std::find(access.begin(), access.end(), u8'$') == access.end())
                return;
            
            auto replaced = replaceInString(access, data);
            node.setContent(replaced.c_str());
            
        } else if (node.type() == XML_ELEMENT_NODE) {
            for (auto prop = node.firstProperty(); prop; prop = prop->nextSibling()) {
                if (prop->firstChild()) {
                    replacePlaceholdersInSelfSiblingsAndChildren(*prop->firstChild(), data);
                }
            }
        }
    }
    
    void replacePlaceholdersInSelfSiblingsAndChildren(XmlNode & node, const ResponseToGet & data) {
        XmlNode * current = &node;
        XmlNode * end = current->parent();
        bool returningFromChild = false;
        while(current != end) {
            if (!returningFromChild) {
                replacePlaceholdersInSelf(*current, data);
                
                if (current->firstChild()) {
                    current = current->firstChild();
                    continue;
                }
            }
            
            returningFromChild = false;
            if (current->nextSibling()) {
                current = current->nextSibling();
                continue;
            }
            
            current = current->parent();
            returningFromChild = true;
        }
    }

private:
    std::optional<sys_string> m_to;
    std::optional<sys_string> m_action;
    std::optional<sys_string> m_relatesTo;
    std::optional<AppSequence> m_appSequence;
    BodyType m_body;
};

class WsdServerImpl final : public WsdServer, UdpServer::Handler, HttpServer::Handler  {
    friend ref_counted<WsdServer>;
private:
    enum RequestType {
        Udp,
        Http
    };
public:
    WsdServerImpl(asio::io_context & ctxt,
                  const refcnt_ptr<Config> & config,
                  HttpServerFactory httpFactory,
                  UdpServerFactory udpFactory,
                  const NetworkInterface & iface,
                  const ip::address & addr):
        WsdServer(iface),
        m_config(config),
        m_iface(iface),
        m_httpAddress(addr, g_WsdHttpPort),
        m_fullComputerName(buildFullComputerName(*config)),
        m_udpServer(udpFactory(ctxt, config, iface, addr)),
        m_httpServer(httpFactory(ctxt, config, iface, m_httpAddress)) {
    }

    void start() override {
        WSDLOG_INFO("Starting WSD server on {}", m_iface);
        if (m_state != NotStarted)
            std::terminate();
        m_udpServer->start(*this);
        m_httpServer->start(*this);
        m_state = Running;
        sendHello();
    }
    
    void stop(bool graceful) override {
        if (m_state == NotStarted)
            std::terminate();
        
        if (m_state == Running) {
            if (graceful) {
                WSDLOG_INFO("WSD on {}: sending Bye", m_iface);
                sendBye();
            } else {
                WSDLOG_INFO("Stopping WSD server on {}", m_iface);
                m_udpServer->stop();
                m_httpServer->stop();
                m_udpServer.reset();
                m_httpServer.reset();
                m_state = Stopped;
            }
        }
    }
    
private:
    ~WsdServerImpl() noexcept {
    }
    
    void onFatalUdpError() override {
        stop(false);
    }
    void onFatalHttpError() override {
        stop(false);
    }

    auto handleUdpRequest(std::unique_ptr<XmlDoc> doc) -> std::optional<XmlCharBuffer> override {
        return handleRequest(Udp, std::move(doc));
    }
    auto handleHttpRequest(std::unique_ptr<XmlDoc> doc) -> std::optional<XmlCharBuffer> override  {
        return handleRequest(Http, std::move(doc));
    }
    
    void sendHello() {
        WSDResponseBuilder builder;
        
        builder.setTo(g_wsdUrn);
        builder.setAction(g_wsdUri + S("/Hello"));
        builder.setAppSequence(WSDResponseBuilder::AppSequence{
            .instanceId = m_config->instanceIdentifier(),
            .messageNumber = m_messageNumber++
        });
        builder.setBody(WSDResponseBuilder::Hello{
            .endpointIdentifier = m_config->endpointIdentifier()
        });
        
        auto doc = builder.build();
        auto buf = doc->dump();
        m_udpServer->broadcast(std::move(buf));
    }
    
    void sendBye() {
        
        WSDResponseBuilder builder;
        
        builder.setTo(g_wsdUrn);
        builder.setAction(g_wsdUri + S("/Bye"));
        builder.setAppSequence(WSDResponseBuilder::AppSequence{
            .instanceId = m_config->instanceIdentifier(),
            .messageNumber = m_messageNumber++
        });
        builder.setBody(WSDResponseBuilder::Bye{
            .endpointIdentifier = m_config->endpointIdentifier()
        });
        
        auto doc = builder.build();
        auto buf = doc->dump();
        
        m_udpServer->broadcast(std::move(buf), [this, holder = refcnt_retain(this)](asio::error_code) {
            stop(false);
        });
    }
    
    auto handleRequest(RequestType type, std::unique_ptr<XmlDoc> doc)  -> std::optional<XmlCharBuffer> {
        auto xpathCtxt = XPathContext::create(*doc);
        xpathCtxt->registerNs(u8"soap", xml_str(g_soapUri));
        xpathCtxt->registerNs(u8"wsa",  xml_str(g_wsaUri));
        xpathCtxt->registerNs(u8"wsd",  xml_str(g_wsdUri));
        
        auto headerNode = xpathCtxt->eval(u8"/soap:Envelope/soap:Header")->firstNode();
        if (!headerNode)
            return std::nullopt;
        
        xpathCtxt->setContextNode(*headerNode);

        sys_string messageId = xpathCtxt->eval(u8"string(./wsa:MessageID)")->stringval();
        if (!checkNewMessageId(messageId)) {
            WSDLOG_DEBUG("repeated message {}, ignoring", messageId);
            return std::nullopt;
        }

        sys_string action = xpathCtxt->eval(u8"string(./wsa:Action)")->stringval();
        const auto & [uri, method] = action.partition_at_last(U'/').value_or(std::pair(S(""), S("")));

        WSDResponseBuilder responseBuilder;
        bool handled = false;
        switch(type) {
        case Udp:
            if (uri == g_wsdUri) {
                if (method == S("Probe")) {
                    WSDLOG_DEBUG("Probe message");
                    handled = handleProbe(*doc, *xpathCtxt, responseBuilder);
                } else if (method == S("Resolve")) {
                    WSDLOG_DEBUG("Resolve message");
                    handled = handleResolve(*doc, *xpathCtxt, responseBuilder);
                } else {
                    WSDLOG_WARN("Unknown UDP message, {}/{}", uri, method);
                }
            }
            break;
        case Http:
            if (uri == g_wsdtUri) {
                if (method == S("Get")) {
                    handled = handleGet(*doc, *xpathCtxt, responseBuilder);
                } else {
                    WSDLOG_WARN("Unknown HTTP message, {}/{}", uri, method);
                }
            }
            break;
        }
        if (!handled)
            return std::nullopt;

        responseBuilder.setTo(g_wsaUri + S("/role/anonymous"));
        responseBuilder.setRelatesTo(messageId);

        auto responseDoc = responseBuilder.build();
        return responseDoc->dump();
    }
    
    auto handleProbe(XmlDoc & doc, XPathContext & xpathCtxt, WSDResponseBuilder & responseBuilder) -> bool {
        
        xpathCtxt.setContextNode(*doc.asNode());
        auto probeNode = xpathCtxt.eval(u8"/soap:Envelope/soap:Body/wsd:Probe")->firstNode();
        if (!probeNode) {
            WSDLOG_WARN("No wsd:Probe in Probe message");
            return false;
        }

        xpathCtxt.setContextNode(*probeNode);
        auto scopesNode = xpathCtxt.eval(u8"./wsd:Scopes")->firstNode();
        if (scopesNode) {
            WSDLOG_WARN("No wsd:Scopes in Probe message");
            return false;
        }

        auto typesNode = xpathCtxt.eval(u8"./wsd:Types")->firstNode();
        if (!typesNode) {
            WSDLOG_WARN("No wsd:Types in Probe message");
            return false;
        }

        sys_string types = typesNode->getContent();
        const auto & [prefix, type] = types.partition_at_first(U':').value_or(std::pair(S(""), S("")));
        if (prefix.empty() || type != S("Device")) {
            WSDLOG_WARN("Invalid type '{}' in Probe message", type);
            return false;
        }

        auto prefixNs = doc.searchNs(*typesNode, xml_str(prefix));
        if (!prefixNs || prefixNs->href() != g_wsdpUri) {
            WSDLOG_WARN("Invalid type prefix '{}' in Probe message", prefix);
            return false;
        }

        responseBuilder.setAction(g_wsdUri + S("/ProbeMatches"));
        responseBuilder.setBody(WSDResponseBuilder::ProbeMatch{
            .endpointIdentifier = m_config->endpointIdentifier()
        });
        responseBuilder.setAppSequence(WSDResponseBuilder::AppSequence{
            .instanceId = m_config->instanceIdentifier(),
            .messageNumber = m_messageNumber++
        });

        return true;
    }
    
    auto handleResolve(XmlDoc & doc, XPathContext & xpathCtxt, WSDResponseBuilder & responseBuilder) -> bool {
        
        xpathCtxt.setContextNode(*doc.asNode());
        sys_string resolveAddr = xpathCtxt.eval(u8"string(/soap:Envelope/soap:Body/wsd:Resolve/wsa:EndpointReference/wsa:Address)")->stringval();
        if (resolveAddr != m_config->endpointIdentifier()) {
            WSDLOG_WARN("No wsa:Address in Resolve message");
            return false;
        }

        responseBuilder.setAction(g_wsdUri + S("/ResolveMatches"));
        responseBuilder.setBody(WSDResponseBuilder::ResolveMatch{
            .endpointIdentifier = m_config->endpointIdentifier(),
            .httpEndpoint = m_httpAddress,
            .httpPath = m_config->httpPath()});
        responseBuilder.setAppSequence(WSDResponseBuilder::AppSequence{
            .instanceId = m_config->instanceIdentifier(),
            .messageNumber = m_messageNumber++
        });

        return true;
    }
    
    auto handleGet(XmlDoc & /*doc*/, XPathContext & /*xpathCtxt*/, WSDResponseBuilder & responseBuilder) -> bool {
        
        responseBuilder.setAction(g_wsdtUri + S("/GetResponse"));
        responseBuilder.setBody(WSDResponseBuilder::ResponseToGet{
            .endpointIdentifier = m_config->endpointIdentifier(),
            .friendlyName = m_config->winNetInfo().hostDescription,
            .fullComputerName = m_fullComputerName,
            .hostAddr = m_httpAddress.address(),
            .metadataTemplate = m_config->metadataDoc()
        });

        return true;
    }

    auto checkNewMessageId(const sys_string & messageId) -> bool {
        auto [it, inserted] = m_knownMessageIds.emplace(messageId);
        if (!inserted)
            return false;
        if (m_knownMessageIdsLRU.size() == g_maxKnownMessages) {
            m_knownMessageIds.erase(m_knownMessageIdsLRU.back());
            m_knownMessageIdsLRU.pop_back();
        }
        m_knownMessageIdsLRU.push_front(it);
        return true;
    }
    
    static auto buildFullComputerName(Config & config) -> sys_string {
        auto & info = config.winNetInfo();

        sys_string_builder fullComputerNameBuilder;
        fullComputerNameBuilder.append(info.hostName);
        std::visit([&](auto & val) {
            using ArgType = std::remove_cvref_t<decltype(val)>;
            
            if constexpr (std::is_same_v<WindowsWorkgroup, ArgType>)
                fullComputerNameBuilder.append(u8"/Workgroup:");
            else if constexpr (std::is_same_v<WindowsDomain, ArgType>)
                fullComputerNameBuilder.append(u8"/Domain:");
            else
                static_assert(makeDependentOn<ArgType>(false), "unhandled type");
            
            fullComputerNameBuilder.append(val.name);
            
        }, info.memberOf);
        
        return fullComputerNameBuilder.build();
    }

private:
    const refcnt_ptr<Config> m_config;
    const NetworkInterface m_iface;
    const ip::tcp::endpoint m_httpAddress;
    const sys_string m_fullComputerName;

    refcnt_ptr<UdpServer> m_udpServer;
    refcnt_ptr<HttpServer> m_httpServer;


    std::set<sys_string> m_knownMessageIds;
    std::deque<std::set<sys_string>::iterator> m_knownMessageIdsLRU;
    size_t m_messageNumber = 0;
};

auto createWsdServer(asio::io_context & ctxt,
                     const refcnt_ptr<Config> & config,
                     HttpServerFactory httpFactory,
                     UdpServerFactory udpFactory,
                     const NetworkInterface & iface,
                     const ip::address & addr) -> refcnt_ptr<WsdServer> {
    
    return refcnt_attach(new WsdServerImpl(ctxt, config, httpFactory, udpFactory, iface, addr));
}
