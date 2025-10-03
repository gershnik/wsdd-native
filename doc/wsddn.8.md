% wsddn(8) WS-Discovery Host Daemon | System Manager's Manual

# NAME

**wsddn** — WS-Discovery Host Daemon

# SYNOPSIS

| `wsddn` `--help`
| `wsddn` `--version`
| `wsddn` [`--unixd`|`--systemd`|`--launchd`] [`-c` _path_] [[`-i` _name_]...] [`-4`|`-6`] [`--hoplimit` _number_] [`--uuid` _uuid_] [`-H` _name_] [`-D`|`-W` _name_] [`--log-level` _level_] [`--log-file` _path_ | `--log-os-log`] [`--pid-file` _path_] [`-U` _user_[:_group_]] [`-r` _dir_]

# DESCRIPTION

`wsddn` is a native daemon that implements WS-Discovery (WSD) host protocol. It allows UNIX hosts to be found by modern Windows versions and displayed in their Explorer Network views. 
`wsddn` can be configured using command-line options or a configuration file (see `--config` option below); command-line options override values specified in the configuration file. `wsddn` rereads its configuration file when it receives a hangup signal, SIGHUP. 

# OPTIONS

Unless otherwise specified options cannot be repeated. The options are as follows:

## General options

`--help`, `-h`
:   Print usage information and exit.

`--version`, `-v`
:   Print version and exit.

`--unixd`
:   Run as a traditional UNIX daemon. 

`--systemd`
:   Run as a systemd daemon. This option is only available on systemd systems.

`--launchd`
:   Run as a launchd daemon. This option is only available on macOS.

`--config` _path_, `-c` _path_
:   Path to the configuration file. The file does not need to exist. If it exists, the file is read and options specified in it are applied _if not already specified on command line_. See [CONFIG FILE] below for information about its syntax and content. Content of the configuration file is re-read upon SIGHUP signal. By default, if this option isn't specified, no configuration file is read.

## Daemon behavior options

`--log-level` _level_     
:   Sets the verbosity of log output. Log levels range from 0 (disable logging) to 6 (detailed trace) with default being 4. Passing values bigger than 6 is equivalent to 6. The equivalent config file option is `log-level`

`--log-file` _path_       
:   The path of the file to write the log output to. If not specified `wsddn` outputs the log messages as follows

    * If invoked without any daemon flags: to standard output
    * If invoked with --systemd: to standard output, with systemd severity prefixes
    * If invoked with --launchd: to standard output
    * If invoked with --unixd: to /dev/null (no logging)

    The equivalent config file option is `log-file`

`--log-os-log`
:   Available on macOS only. If specified log output is sent to system log visible via Console app or `log` command line tool. This option is mutually exclusive with `--log-file`.

`--pid-file` _path_       
:   Set the path to PID file. If not specified no PID file is written. Send SIGHUP signal to the process ID in the PID file to reload 
configuration. If --user option is used, the directory of the pidfile must allow the specified user to create and delete files. The equivalent config file option is `pid-file`

`--user` _username_\[:_groupname_\], `-U` _username_\[:_groupname_\]
:   Set the identity under which the process that performs network communications will run. If group name is not specified, primary group of the username is used.
    
    If this option is not specified then the behavior is as follows:
    
    - If wsddn process is run under the root account it tries to use a special unprivileged account name (`_wsddn`:`_wsddn` on macOS, `wsddn`:`wsddn` otherwise) The user and group are created if they do not exist. Any failures in these steps stop the process.
    - Otherwise, wsddn uses the account it is run under.
    
    The net effect of these rules is that wsddn under no circumstances will perform network communications under root account. The equivalent config file option is `user`

`--chroot` _dir_, `-r` _dir_  
:   Set the directory into which process that performs network communications should chroot into. This further limits any impact of potential network
exploits into wsddn. If not specified the behavior is as follows
  
    - If wsddn process is run under the root account: use `/var/empty` on macOS and `/var/run/wsddn` on other platforms. This directory will be created if it does not exist. 
    - Otherwise: do not chroot
  
    Note: do not use external methods to chroot wsddn process (e.g. using launchd config plist). Non-networking parts of it need access to the normal filesystem to detect things like SMB configuration etc. The equivalent command line option is `chroot`

## Networking options

`--interface` _name_, `-i` _name_
:   Network interface to use. Pass this option multiple times for multiple interfaces. If not specified, all eligible interfaces will be used. Loop-back interfaces are never used, even when explicitly specified. For interfaces with IPv6 addresses, only link-local addresses will be used. The equivalent config file option is `interfaces`

`--ipv4only`, `-4`        
:   Use only IPv4. By default both IPv4 and IPv6 are used. This option cannot be combined with `--ipv6only`. The equivalent config file option is `allowed-address-family`.
  
`--ipv6only`, `-6`        
:   Use only IPv6. By default both IPv4 and IPv6 are used. This option cannot be combined with `--ipv4only`. The equivalent config file option is `allowed-address-family`.
  
`--hoplimit` _number_
:   Set the hop limit for multicast packets. The default is 1 which should prevent packets from leaving the local network segment. The equivalent config file option is `hoplimit`

`--source-port` _number_
:   Set the source port for outgoing multicast messages, so that replies will use this as the destination port. This is useful for firewalls that do not detect incoming unicast replies to a multicast as part of the flow, so the port needs to be fixed in order to be allowed manually.

## Machine information options

`--uuid` _uuid_
:   WS-Discovery protocol requires your machine to have a unique identifier that is stable across reboots or changes in networks.  By default, `wsddn` uses UUID version 5 with private namespace and the host name of the machine. This will remain stable as long as the hostname doesn't change. If desired, you can override this with a fixed UUID using this option. The equivalent config file option is `uuid`

`--hostname` _name_, `-H` _name_
:   Override hostname to be reported to Windows machines. By default the local machine's hostname (with domain part, if any, removed) is used. If you set the name to `:NETBIOS:` then Netbios hostname will be used. The Netbios hostname is either detected from SMB configuration, if found, or produced by capitalizing normal machine hostname. The equivalent config file option is `hostname`

`--domain` _name_, `-D` _name_
:   Report this computer as a member of Windows domain _name_. Options `--domain` and `--workgroup` are mutually exclusive. If neither is specified domain/workgroup membership is auto-detected from SMB configuration. If no SMB configuration is found it is set to a workgroup named `WORKGROUP`. The equivalent config file option is `member-of`. 

`--workgroup` _name_, `-W` _name_
:   Report this computer as a member of Windows workgroup _name_. Options `--domain` and `--workgroup` are mutually exclusive. If neither is specified domain/workgroup membership is auto-detected from SMB configuration. If no SMB configuration is found it is set to a workgroup named `WORKGROUP`. The equivalent config file option is `member-of`. 

`--smb-conf` _path_
:   Path to `smb.conf`, `samba.conf`, or `ksmbd.conf` file to extract the SMB configuration from. This option is not available on macOS. By default `wsddn` tries to locate this file on its own by querying your local Samba installation. Use this option if auto-detection fails, picks wrong Samba instance or if you are using KSMBD on Linux. The equivalent config file option is `smb-conf`.

`--metadata` _path_, `-m` _path_
:   Path to a custom metadata XML file. Custom metadata allows you to completely replace the information normally supplied by `wsddn` to Windows with your own. See <https://github.com/gershnik/wsdd-native/blob/master/config/metadata/README.md> for details about the metadata format and content.


# SIGNALS

`wsddn` handles the following signals:

`SIGHUP`
: gracefully stop network communications, reload configuration and re-start communications.

`SIGTERM`, `SIGINT`
: gracefully stop network communications and exit. 

# EXIT STATUS

`wsddn` exit code is 0 upon normal termination (via `SIGINT` or `SIGTERM`) or non-zero upon error. 

# FIREWALL SETUP

Traffic for the following ports, directions and addresses must be allowed:

- Incoming and outgoing traffic to udp/3702 with multicast destination: 239.255.255.250 for IPv4 and ff02::c for IPv6
- Outgoing unicast traffic from udp/3702
- Incoming traffic to tcp/5357

You should further restrict the traffic to the (link-)local subnet, e.g. by using the `fe80::/10` address space for IPv6. Please note that IGMP traffic must be enabled in order to get IPv4 multicast traffic working.

# CONFIG FILE

The syntax of the configuration file is TOML (<https://toml.io/en/>). 

Any options specified on command line take precedence over options in the config file. Most options are named and behave exactly the same as corresponding command line options. Exceptions are explained in-depth below.

`allowed-address-family` = "IPv4" | "IPv6"
: Restrict communications to the given address family. Valid values are "IPv4" or "IPv6" case-insensitive. The equivalent command line options are `--ipv4only` and `--ipv6only`

`chroot` = "path"
: Same as `--chroot` command line option

`hoplimit` = _number_
: Same as `--hoplimit` command line option

`source-port` = _number_
: Same as `--source-port` command line option

`hostname` = "_name_"
: Same as `--hostname` command line option

`interfaces` = [ "_name_", ... ]
: Specify on which interfaces wsddn will be listening on. If no interfaces are specified, or the list is empty all suitable detected interfaces will be used. Loop-back interfaces are never used, even when explicitly specified. For interfaces with IPv6 addresses, only link-local addresses will be used. The equivalent command line option is `--interface`

`log-level` = _number_
: Same as `--log-level` command line option

`log-file` = "path"
: Same as `--log-file` command line option

`log-os-log` = true/false
: This setting is only available on macOS. Setting it to `true` is the same as `--log-os-log` command line option

`member-of` = "Workgroup/_name_" | "Domain/_name_"
: Report whether the host is a member of a given workgroup or domain. To specify a workgroup use "Workgroup/name" syntax. To specify a domain use "Domain/name". The "workgroup/" and "domain/" prefixes are not case sensitive. If not specified workgroup/domain membership is detected from SMB configuration. If no SMB configuration is found it is set to a workgroup named WORKGROUP. The equivalent command line options are `--domain` and `--workgroup`.

`pid-file` = "path"
: Same as `--pid-file` command line option

`smb-conf` = "path"
: Same as `--smb-conf` command line option

`metadata` = "path"
: Same as `--metadata` command line option

`user` = "username\[:groupname\]"
: Same as `--user` command line option

`uuid` = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
: Same as `--uuid` command line option

# EXAMPLES

## Run as a traditional Unix daemon 

```
wsddn --unixd --config=/usr/local/etc/wsddn.conf --pid-file=/var/run/wsddn/wsddn.pid --log-file=/var/log/wsddn.log
```

## Run as a systemd daemon

```
wsddn --systemd --config=/etc/wsddn.conf
```

## Handle traffic on eth0 and eth2 only, but only with IPv6 addresses

```
wsddn -i eth0 -i eth2 -6
```

# AUTHOR 
Eugene Gershnik <gershnik@hotmail.com>

# BUG REPORTS
Report bugs at <https://github.com/gershnik/wsdd-native/issues>.
