# wsdd-native

![Linux](https://img.shields.io/badge/Linux-FCC624?style=flat&logo=linux&logoColor=black)
![macOS](https://img.shields.io/badge/-macOS-black?style=flat&logo=apple&logoColor=F0F0F0)
![FreeBSD](https://img.shields.io/badge/-FreeBSD-%23870000?style=flat&logo=freebsd&logoColor=white)
[![Language](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)
[![Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
[![License](https://img.shields.io/badge/license-BSD-brightgreen.svg)](https://opensource.org/licenses/BSD-3-Clause)

A Unix daemon that makes your Linux/macOS/BSD machine visible in Network view of Windows Explorer on newer versions of Windows.

It implements WS-Discovery protocol that Windows now uses to discover machines on local network. It is a native daemon, written in C++. 

<!-- Links -->

[manpage]: https://htmlpreview.github.io/?https://github.com/gershnik/wsdd-native/blob/master/doc/wsddn.8.html
[releases]: https://github.com/gershnik/wsdd-native/releases
[issues]: https://github.com/gershnik/wsdd-native/issues
[wsdd]: https://github.com/christgau/wsdd
[wsdd2]: https://github.com/Netgear/wsdd2
[chroot_jail]: https://en.wikipedia.org/wiki/Chroot
[macports]: https://www.macports.org
[homebrew]: https://brew.sh

<!-- End Links -->

<!-- TOC depthfrom:2 -->

- [Features](#features)
- [Binary packages](#binary-packages)
    - [Ubuntu/Debian/Mint/Raspberry Pi](#ubuntudebianmintraspberry-pi)
    - [RedHat/CentOS/Fedora](#redhatcentosfedora)
    - [FreeBSD](#freebsd)
    - [macOS](#macos)
        - [Standalone installer](#standalone-installer)
        - [Homebrew](#homebrew)
        - [Macports](#macports)
- [Building from sources](#building-from-sources)
    - [Prerequisites](#prerequisites)
    - [Building and installing](#building-and-installing)
    - [Setting up daemon](#setting-up-daemon)
- [Usage](#usage)
    - [Firewall Setup](#firewall-setup)
    - [Security](#security)
    - [Custom metadata](#custom-metadata)
- [Acknowledgements](#acknowledgements)
- [Reporting Bugs](#reporting-bugs)

<!-- /TOC -->

## Features

* Fully supports macOS, Linux and FreeBSD
* Can be configured via a configuration file, not just command line.
* Discovers Samba/macOS SMB configuration on its own. (This can be overridden, if desired)
* Can present the Unix host as something other than "Computer" in Windows Explorer.
* Integrates well with `systemd` and `launchd`. Of course it can also run as a classical Unix daemon for other init systems.
* Friendly to various log rotation methods like `newsyslogd` and `logrotate`. Supports standard reload semantics via SIGHUP.
* Written with security in mind first and foremost. 
* Will never run any network code as root. Designated user account to run under is created automatically, if needed.

There are a couple of similar projects available: [wsdd][wsdd] written in Python and [wsdd2][wsdd2] written in C. Neither of them, however, fully provides the features above. 

The biggest drawback of **wsdd-native** compared to these projects is that it requires modern (as of 2022) C++ compiler to build and set of modern `libc`, `libstdc++`/`libc++` etc. to run. This usually limits it to recent versions of operating systems it supports. (It is possible to build it on older ones with newer toolchains but doing so is non-trivial). Of course, as time passes this limitation will become less and less significant.

## Binary packages

### Ubuntu/Debian/Mint/Raspberry Pi 

Pre-built packages are available in a custom apt repository for systems newer than Ubuntu 20.04 (focal) or
Debian 11 (bullseye). Any Debian system based upon those or newer should work.

Architectures supported: `amd64` (aka `x86_64`), `arm64` (aka `aarch64`) and `armhf` 

To set up the apt repository:

* Import the repository public key
  ```bash
  wget -qO- https://www.gershnik.com/apt-repo/conf/pgp-key.public \
    | gpg --dearmor \
    | sudo tee /usr/share/keyrings/gershnik.gpg >/dev/null
  ```

* Add new repo
  ```bash
  echo "deb" \
  "[arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/gershnik.gpg]" \
  "https://www.gershnik.com/apt-repo/" \
  "base" \
  "main" \
    | sudo tee /etc/apt/sources.list.d/wsddn.list >/dev/null
  ```

Once the repository is set up you can install `wsddn` as usual via:

```bash
sudo apt update
sudo apt install wsddn
```

If you have UFW firewall running do

```bash
sudo ufw allow wsddn
```

Daemon will be enabled and started automatically on first install but keep its existing state on updates. To start/stop/reload it use

```bash
sudo systemctl start wsddn
sudo systemctl stop wsddn
sudo systemctl reload wsddn
```

Configuration file will be at `/etc/wsddn.conf`. Comments inside indicate available options and their meaning. 
You can also use `man wsddn` to learn about configuration or see online version [here][manpage]

Daemon log can be viewed via `journalctl` as usual

```bash
journalctl -u wsddn
```

### RedHat/CentOS/Fedora

Pre-built packages are available [Fedora Copr](https://copr.fedorainfracloud.org/coprs/gershnik/wsddn/) repository.
Visit that link to see currently supported distributions and architectures. 

To set the repo up you need to install `copr` plugin if you haven't already done so:

```bash
sudo dnf install dnf-plugins-core
#or with yum
#sudo yum install yum-plugin-copr
```

Then

```bash
sudo dnf copr enable gershnik/wsddn
#or with yum
#sudo yum copr enable gershnik/wsddn
```

Once the repository is set up you can install `wsddn` as usual via

```bash
sudo dnf install wsddn
#or with yum
#sudo yum install wsddn
```

On first install firewall ports `5357/tcp` and `3702/udp` will be opened. 

Enable and start the daemon:

```bash
sudo systemctl enable wsddn
sudo systemctl start wsddn
```

To start/stop/reload it use

```bash
sudo systemctl start wsddn
sudo systemctl stop wsddn
sudo systemctl reload wsddn
```

Configuration file will be at `/etc/wsddn.conf`. Comments inside indicate available options and their meaning. 
You can also use `man wsddn` to learn about configuration or see online version [here][manpage]

Daemon log can be viewed via `journalctl` as usual

```bash
journalctl -u wsddn
```


### FreeBSD

Pre-built packages are available for FreeBSD 13 and 14 in a custom binary package repository. 
Both `amd64` (aka `x86_64`) and `arm64` (aka `aarch64`) architectures are supported.

To set the repo up:

* Create the custom repo config folder if it does not already exist
  ```bash
  sudo mkdir -p /usr/local/etc/pkg/repos
  ```
* Download the repository public key
  ```bash
  wget -qO- https://www.gershnik.com/bsd-repo/rsa-key.pub \
   | sudo tee /usr/local/etc/pkg/repos/www_gershnik_com.pub > /dev/null
  ```
* Create a file named `/usr/local/etc/pkg/repos/www_gershnik_com.conf`
* Put the following content in it:
  ```
  www_gershnik_com: {
      url: "https://www.gershnik.com/bsd-repo/${ABI}",
      signature_type: "pubkey",
      pubkey: "/usr/local/etc/pkg/repos/www_gershnik_com.pub",
      enabled: yes
  }
  ```

Once the repository is set up you can install `wsddn` as usual via

```bash
sudo pkg update
sudo pkg install wsddn
```

As is standard on FreeBSD daemon will not be enabled or started after installation. To enable it, edit `/etc/rc.conf` and add
the following line there:
```
wsddn_enable="YES"
```

To start/stop/reload the daemon use:

```bash
sudo service wsddn start
sudo service wsddn stop
sudo service wsddn reload
```

Configuration file will be at `/usr/local/etc/wsddn.conf`. Comments inside indicate available options and their meaning. 
You can also use `man wsddn` to learn about configuration or see online version [here][manpage]

Log file is located at `/var/log/wsddn.log`. Log file rotation is configured via `newsylogd`. To modify rotation settings edit `/usr/local/etc/newsyslog.conf.d/wsddn.conf`

### macOS

On macOS there are 3 ways to install `wsddn`: via a standalone installer package, [Homebrew][homebrew] or [Macports][macports]. 
Using a standalone installer is simpler but you will have to manually install any future updates as well.
Homebrew/Macports are a bit more complicated to set up (and Macports also requires Xcode to be present) but it provides updatability 
similar to Linux package managers. 

For all 3 methods the supported platforms are:
- macOS Catalina (10.15) and above
- Both Intel and Apple Silicon

#### Standalone installer

To install via standalone `.pkg` installer:

* Navigate to [Releases][releases] page.
* Expand the release you wish to install and download `wsddn-macos-x.x.pkg`
* Launch it to run GUI installer or, if you prefer, install on command line via
  ```bash
  sudo installer -pkg /path/to/wsddn-macos-x.x.pkg -target /
  ```

To fully uninstall `wsddn` run `/usr/local/bin/wsddn-uninstall`

Daemon will start automatically on install. 

To start/stop/reload the daemon use:

```bash
sudo launchctl kickstart system/io.github.gershnik.wsddn
sudo launchctl kill TERM system/io.github.gershnik.wsddn
sudo launchctl kill HUP system/io.github.gershnik.wsddn
```

Configuration file will be at `/etc/wsddn.conf`. Comments inside indicate available options and their meaning. 
You can also use `man wsddn` to learn about configuration or see online version [here][manpage]

Daemon and related logs can be viewed in system log by searching for subsystem or
process names containing string `wsddn`. For example:

```bash
log show --last 15m --debug --info \
  --predicate 'subsystem CONTAINS "wsddn" OR process CONTAINS "wsddn"'
```

#### Homebrew

Homebrew package ('cask') can be installed via a custom tap. 

To set it up

```bash
brew tap gershnik/repo
```

Then

```bash
brew install wsddn
```

This installs exactly the same thing as standalone installer would so all the usage instructions [above](#standalone-installer) apply as well.


#### Macports

Macports package can be installed via a custom repository.

To set the repo up:

```bash
sudo bash <<'___'
set -e
pemurl=https://gershnik.com/macports-repo/macports.pem
porturl=https://www.gershnik.com/macports-repo/ports.tar.bz2
prefix=$(dirname $(dirname $(which port)))
pemfile="$prefix/share/macports/gershnik.pem"
pubkeysfile="$prefix/etc/macports/pubkeys.conf"
sourcesfile="$prefix/etc/macports/sources.conf"
curl -s $pemurl > "$pemfile"
grep -qxF "$pemfile" "$pubkeysfile" || echo $pemfile >> "$pubkeysfile"
grep -qxF "$porturl" "$sourcesfile" || echo $porturl >> "$sourcesfile"
sudo port sync
___
```

Then you can install `wsddn` as usual via

```bash
sudo port install wsddn
```

Daemon will start automatically on install. 

To start/stop/reload the daemon use:

```bash
sudo launchctl kickstart system/org.macports.wsddn
sudo launchctl kill TERM system/org.macports.wsddn
sudo launchctl kill HUP system/org.macports.wsddn
```

Configuration file will be at `/opt/local/etc/wsddn.conf`. Comments inside indicate available options and their meaning. 
You can also use `man wsddn` to learn about configuration or see online version [here][manpage]

Daemon and related logs can be viewed in system log by searching for subsystem or
process names containing string `wsddn`. For example:

```bash
log show --last 15m --debug --info \
  --predicate 'subsystem CONTAINS "wsddn" OR process CONTAINS "wsddn"'
```

## Building from sources

### Prerequisites

* Git
* C++20 capable compiler. Minimal compilers known to work are GCC 10.2, Clang 13 and Xcode 13.
* CMake 3.25 or greater. If your distribution CMake is older than that you can download a newer version from https://cmake.org/download/
* Optional: On Linux if you wish to enable `systemd` integration make sure you have `libsystemd` library and headers installed on your system. On APT systems use:
  ```bash
  sudo apt install libsystemd-dev
  ```
  On DNF systems use
  ```bash
  sudo dnf install systemd-devel
  ```

### Building and installing

```bash
git clone https://github.com/gershnik/wsdd-native.git
cd wsdd-native
cmake -S . -B out -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFETCHCONTENT_QUIET=OFF ..
cmake --build  out
sudo cmake --install out --strip
```

The `wsddn` executable will be installed into `/usr/local/bin` and manpage added to section 8 of the manual.

The following flags can be passed to CMake configure step:

`-DWSDDN_PREFER_SYSTEM=ON|OFF` 

This controls whether to prefer system version of 3rd party libraries or fetch, build and use static ones. Currently only `libxml2` is affected.

On Linux:

`-DWSDDN_WITH_SYSTEMD="yes"|"no"|"auto"`. 

This controls whether to enable `systemd` integration. Auto performs auto-detection (this is the default). 

### Setting up daemon

The [config](config) directory of this repo contains sample configuration files for different init systems (systemd, FreeBSD rc.d and macOS launchd). You can adapt those as appropriate to your system. 

Command line flags and configuration file entries are documented in `man wsddn` and online [here][manpage]


## Usage

### Firewall Setup

<small>Note: The following instructions are copied verbatim from [wsdd][wsdd] one since the requirements are identical</small>

Traffic for the following ports, directions and addresses must be allowed.

* incoming and outgoing traffic to `udp/3702` with multicast destination:
  * `239.255.255.250` for IPv4
  * `ff02::c` for IPv6
* outgoing unicast traffic from `udp/3702`
* incoming to `tcp/5357`

You should further restrict the traffic to the (link-)local subnet, e.g. by using the `fe80::/10` address space for IPv6. Please note that IGMP traffic must be enabled in order to get IPv4 multicast traffic working.

### Security

There are two main security concerns with a daemon that delivers data about local machine over the network

1. A bug inside daemon code may allow remote attacker to penetrate the machine running it.
2. The information legitimately provided by the daemon will disclose something to an attacker that would otherwise remain unknown, enabling him to mount further attacks.

Currently the implementation ignores the second concern. The things **wsdd-native** discloses are the existence of the local host, its name, presence of Samba on it and domain/workgroup membership. All of these are generally disclosed by Samba itself via SMB broadcasts so, assuming the firewall is configured as described above, there is no net gain for an attacker. WS-Discovery protocol contains provisions for encrypting its HTTP traffic and potentially authenticating clients accessing your host via their client certificates. This limits exposure somewhat but at a significant configuration and maintenance cost. If there is interest in any of it it is possible to easily add this functionality in a future version. 

The first concern is by far the most significant one. All software contains bugs and despite developer's best efforts there is always a risk that a bad actor can discover some kind of input that allows him to hijack the server process. To address this possibility **wsdd-native** takes the following measures (apart from general secure coding practices):
* The process performing network communications never runs as root. If launched as root it will create an unprivileged account (`_wsddn:_wsddn` on macOS and `wsddn:wsddn` on other platforms) and run network process under it.
* Similarly when started as root the daemon will lock the network process in a [chroot jail][chroot_jail] (`/var/empty` on macOS and `/var/run/wsddn` on other platforms). 

These measures are automatic and cannot be bypassed. Taken together they should limit the fallout of any vulnerability though, of course, nothing ever can be claimed to be 100% secure.

Note that when running on `systemd` systems it is recommended to use its `DynamicUser` facility instead of running as root and relying on the measures above. The Debian/Ubuntu installer does so.

### Custom metadata

By default **wsdd-native** exposes the host it is running on as a computer in "Computer" section of Windows Explorer Network view. Clicking on a computer will attempt to access its shares via SMB protocol.

Instead of this **wsdd-native** allows you to expose the host as a different kind of device among those supported by Windows Explorer, for example a media player, home security, printer etc. To do so you need to author a custom metadata XML and specify it via `--metadata` command line switch or `metadata` field in `wsddn.conf`.

More details on this can be found [on this page](config/metadata/README.md).


## Acknowledgements

**wsdd-native** is directly influenced by [wsdd][wsdd]. While no source code from it was directly re-used in this project, many design and implementation ideas were; as well as command line design and some documentation content.

See [Acknowledgements.md](Acknowledgements.md) for information about open source libraries used in this project.

## Reporting Bugs

Please use the GitHub [issue tracker][issues] to report any bugs or suggestions.
For vulnerability disclosures or other security concerns see [Security Policy](SECURITY.md)

