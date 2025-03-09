# wsdd-native

![Linux](https://img.shields.io/badge/Linux-FCC624?style=flat&logo=linux&logoColor=black)
![macOS](https://img.shields.io/badge/-macOS-black?style=flat&logo=apple&logoColor=F0F0F0)
![FreeBSD](https://img.shields.io/badge/-FreeBSD-%23870000?style=flat&logo=freebsd&logoColor=white)
![OpenBSD](https://img.shields.io/badge/OpenBSD-e6ffff?style=flat&logo=openbsd&logoColor=FF0000)
![NetBSD](https://img.shields.io/badge/NetBSD-fefefe?style=flat&logo=netbsd)
[![Language](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)
[![Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
[![License](https://img.shields.io/badge/license-BSD-brightgreen.svg)](https://opensource.org/licenses/BSD-3-Clause)

A Unix daemon that makes your Linux/macOS/BSD/illumos machine visible in Network view of Windows Explorer on newer versions of Windows.

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
[aur]: https://wiki.archlinux.org/title/Arch_User_Repository

<!-- End Links -->

<!-- TOC depthfrom:2 -->

- [Features](#features)
- [Binary packages](#binary-packages)
    - [macOS](#macos)
        - [Standalone installer](#standalone-installer)
        - [Homebrew](#homebrew)
        - [Macports](#macports)
    - [Ubuntu/Debian/Mint/Raspberry Pi](#ubuntudebianmintraspberry-pi)
    - [RedHat/CentOS/Fedora](#redhatcentosfedora)
    - [OpenSUSE](#opensuse)
    - [Arch Linux](#arch-linux)
    - [Alpine](#alpine)
    - [FreeBSD](#freebsd)
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

* Fully supports macOS, Linux, FreeBSD, OpenBSD, NetBSD and illumos
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

### macOS

On macOS there are 3 ways to install `wsddn`: via a standalone installer package, [Homebrew][homebrew] or [Macports][macports]. 
Using a standalone installer is simpler but you will have to manually install any future updates as well.
Homebrew/Macports are a bit more complicated to set up but it provides updatability similar to Linux/BSD package managers. 

For all 3 methods the supported platforms are:
- macOS Catalina (10.15) and above
- Both Intel and Apple Silicon

#### Standalone installer

<details>

<summary>Setup and usage (click to expand)</summary>
<br>

To install via standalone `.pkg` installer:

* Download [the installer package](https://github.com/gershnik/wsdd-native/releases/download/v1.17/wsddn-macos-1.17.pkg)
* Double click it to run and follow the prompts.

If you prefer command line, you can also install via:
```bash
sudo installer -pkg /path/to/wsddn-macos-1.17.pkg -target /
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

</details>

#### Homebrew

<details>

<summary>Setup and usage (click to expand)</summary>
<br>

Homebrew package ('cask') can be installed via a custom tap. 

To set it up

```bash
brew tap gershnik/repo
```

Then

```bash
brew install wsddn
```

This installs exactly the same thing as standalone installer would so all the usage instructions under [Standalone installer](#standalone-installer) apply as well.

</details>


#### Macports

<details>

<summary>Setup and usage (click to expand)</summary>
<br>

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

</details>


### Ubuntu/Debian/Mint/Raspberry Pi 

Pre-built packages are available in a custom apt repository for systems newer than Ubuntu 20.04 (focal) or
Debian 11 (bullseye). Any Debian system based upon those or newer should work.

Architectures supported: `amd64` (aka `x86_64`), `arm64` (aka `aarch64`) and `armhf` 

<details>

<summary>Setup and usage (click to expand)</summary>
<br>

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

If you have UFW firewall running, do

```bash
sudo ufw allow wsddn
```

Daemon will be enabled and started automatically on first install but keep its existing state on updates. 

On `systemd` based distributions to start/stop/reload it use

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

On non-`systemd` based distributions (and Docker) you can use:

```bash
sudo /etc/init.d/wsddn start
sudo /etc/init.d/wsddn stop
sudo /etc/init.d/wsddn reload
```

and the log is available at `/var/log/wsddn.log`

</details>

### RedHat/CentOS/Fedora

Pre-built packages are available on [Fedora Copr](https://copr.fedorainfracloud.org/coprs/gershnik/wsddn/) repository.
Visit that link to see currently supported distributions and architectures. 

<details>

<summary>Setup and usage (click to expand)</summary>
<br>

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

On first install firewall will be configured to open `wsddn` service. 

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

</details>

### OpenSUSE

Pre-built OpenSUSE packages for Tumbleweed are available via [Fedora Copr](https://copr.fedorainfracloud.org/coprs/gershnik/wsddn/). 

Architectures supported: `x86_64` and`aarch64`

<details>

<summary>Setup and usage (click to expand)</summary>
<br>

To set up the repository:

1. Import the repository PGP key
  ```bash
  wget -qO wsddn.gpg  https://download.copr.fedorainfracloud.org/results/gershnik/wsddn/pubkey.gpg \
    && sudo rpm --import wsddn.gpg \
    && rm wsddn.gpg
  ```
2. Add the repository configuration to `zypper`
  ```bash
  sudo zypper addrepo -f https://download.copr.fedorainfracloud.org/results/gershnik/wsddn/opensuse-tumbleweed-$(arch) wsddn
  ```
3. Refresh `zypper`
  ```bash
  sudo zypper refresh
  ```
  You will receive a warning saying something like `Warning: File 'repomd.xml' from repository 'wsddn' is unsigned ...`. This is expected as Fedora Copr doesn't sign the repository itself, only actual RPMs.
  Answer `y` to allow.

Once the repository is set up you can install `wsddn` as usual via:

```bash
sudo zypper in wsddn
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

</details>

### Arch Linux

Source package is available on [AUR][aur] at https://aur.archlinux.org/packages/wsdd-native 

Pre-built packages are available in a custom `pacman` repository.

Architectures supported: `x86_64` and`aarch64`

<details>

<summary>Setup and usage (click to expand)</summary>
<br>

To set up the repository:

1. Import the repository PGP key
  ```bash
  sudo pacman-key --recv-keys 'CF567A58C5DB9C6908C6E87E6EBB54370005A361' \
    && sudo pacman-key --lsign-key 'CF567A58C5DB9C6908C6E87E6EBB54370005A361'
  ```
2. Add the repository configuration to `/etc/pacman.conf`
  ```ini
  [gershnik]
  Server = https://www.gershnik.com/arch-repo/$arch
  ```
3. Refresh the package databases
  ```bash
  sudo pacman -Sy
  ```

Once the repository is set up you can install `wsdd-native` as usual via:

```bash
sudo pacman -S wsdd-native
```

If you have UFW firewall running, do

```bash
sudo ufw allow wsddn
```

As per Arch Linux convention the installation does not automatically enable or start services.

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

</details>

### Alpine

Pre-built packages are available in a custom `apk` repository for Alpine 3.18 or above. 

Architectures supported: `x86_64` and`aarch64`

<details>

<summary>Setup and usage (click to expand)</summary>
<br>

To set up the repository:

1. Import the repository key
  ```bash
  wget -qO-  https://www.gershnik.com/alpine-repo/gershnik@hotmail.com-6643812b.rsa.pub \
    | sudo tee /etc/apk/keys/gershnik@hotmail.com-6643812b.rsa.pub >/dev/null
  ```

2. Add new repo
  ```bash
  sudo mkdir -p /etc/apk/repositories.d && \
  echo "https://www.gershnik.com/alpine-repo/main" \
    | sudo tee /etc/apk/repositories.d/www.gershnik.com.list >/dev/null
  ```

3. Update `apk`
  ```bash
  sudo apk update
  ```

4. Install the package. 
  ```bash
  sudo apk add wsdd-native
  ```
  
If your Alpine system has OpenRC running (e.g. not a Docker container), OpenRC configuration will be automatically installed too.
Otherwise, if desired, you can manually add it via `sudo apk add wsdd-native-openrc`. Similarly documentation is available via `sudo apk add wsdd-native-doc`.

Under OpenRC:

Enable and start the service:

```bash
sudo rc-update add wsddn
sudo rc-service wsddn start
```

To start/stop/reload it use

```bash
sudo rc-service wsddn start
sudo rc-service wsddn stop
sudo rc-service wsddn reload
```

Configuration file will be at `/etc/wsddn.conf`. Comments inside indicate available options and their meaning. 
If you installed documentation you can also use `man wsddn` to learn about configuration or see online version [here][manpage]

Log file is located at `/var/log/wsddn.log`. Log file rotation is configured via `logrotate`. To modify rotation settings edit `/etc/logrotate.d/wsddn`

</details>

### FreeBSD

Pre-built packages are available for FreeBSD 13 and 14 in a custom binary package repository. 
Both `amd64` (aka `x86_64`) and `arm64` (aka `aarch64`) architectures are supported.

<details>

<summary>Setup and usage (click to expand)</summary>
<br>

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

</details>

## Building from sources

### Prerequisites

* Git
* C++20 capable compiler. Minimal compilers known to work are GCC 10.2, Clang 13 and Xcode 13.
* CMake 3.25 or greater. If your distribution CMake is older than that you can download a newer version from https://cmake.org/download/
* `patch` tool. Most operating system distributions have it available by default by some minimalistic ones might not.
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

<small>Note: The following instructions are copied almost verbatim from [wsdd][wsdd] since the requirements are identical</small>

Traffic for the following ports, directions and addresses must be allowed.

* incoming and outgoing traffic to `udp/3702` with multicast destination:
  * `239.255.255.250` for IPv4
  * `ff02::c` for IPv6
* outgoing unicast traffic from `udp/3702`
* incoming to `tcp/5357`

You should further restrict the traffic to the (link-)local subnet, e.g. by using the `fe80::/10` address space for IPv6. Please note that IGMP traffic must be enabled in order to get IPv4 multicast traffic working.

For UFW and firewalld, application/service profiles can be found under `config/firewalls`. If using binary installation packages these are provided 
as part of the installation. Note that UFW profiles only allow to grant the traffic on specific UDP and TCP ports, but a restriction on the IP range (like link local for IPv6) or the multicast traffic is not possible.

### Security

There are four main security concerns with a daemon that accepts network requests and delivers data about local machine over the network

1. A bug inside the daemon code may allow a remote attacker to penetrate the machine running it.
2. The information legitimately provided by the daemon will disclose something to an attacker that would otherwise remain unknown, enabling him to mount further attacks.
3. A bug or even the _normal functionality_ of the daemon might allow a remote attacker to use it to mount further attacks against other systems. For example it might be possible to "convince" the daemon to become a part of a distributed denial of service (DDoS) attack.
4. A bug or a normal operation of the daemon might allow a remote attacker to make it or even the entire machine hosting it unresponsive resulting in a denial of service.

Currently the implementation ignores the second concern. The things **wsdd-native** discloses are the existence of the local host, its name, presence of Samba on it and domain/workgroup membership. All of these are generally disclosed by Samba itself via SMB broadcasts so, assuming the firewall is configured as described above, there is no net gain for an attacker. WS-Discovery protocol contains provisions for encrypting its HTTP traffic and potentially authenticating clients accessing your host via their client certificates. This limits exposure somewhat but at a significant configuration and maintenance cost. If there is interest in any of it it is possible to easily add this functionality in a future version. 

The first concern is by far the most significant one. All software contains bugs and despite developer's best efforts there is always a risk that a bad actor can discover some kind of input that allows him to hijack the server process. To address this possibility **wsdd-native** takes the following measures (apart from general secure coding practices):
* The process performing network communications never runs as root. If launched as root it will create an unprivileged account (`_wsddn:_wsddn` on macOS and `wsddn:wsddn` on other platforms) and run network process under it.
* Similarly when started as root the daemon will lock the network process in a [chroot jail][chroot_jail] (`/var/empty` on macOS and `/var/run/wsddn` on other platforms). 

These measures are automatic and cannot be bypassed. Taken together they should limit the fallout of any vulnerability though, of course, nothing ever can be claimed to be 100% secure.

Note that when running on `systemd` systems it is recommended to use its `DynamicUser` facility instead of running as root and relying on the measures above. The Debian/Ubuntu/Fedora/Arch binary packages do so.

The third concern is also a significant one. Even in absence of any bugs a completely correct implementation of WS-Discovery protocol is known to be vulnerable to these kinds of attacks. See for example 
[here](https://blogs.akamai.com/sitr/2019/09/new-ddos-vector-observed-in-the-wild-wsd-attacks-hitting-35gbps.html) and 
[here](https://www.zdnet.com/article/protocol-used-by-630000-devices-can-be-abused-for-devastating-ddos-attacks/). 
Bugs (always a possibility) can make things even worse. As far as I know there is no effective mitigation to this threat that **wsdd-native** can implement in code. The only way to prevent these kinds of attacks is to __never__ expose **wsdd-native** ports to open internet via [firewall configuration](#firewall-setup). Given that the whole purpose of this daemon is to enable interoperability with Windows via SMB protocol there is probably never a good
reason to let it accept and send traffic outside of a local network.

The fourth concern, while also present, is less severe than the above. **wsdd-native** is single threaded and so, even if overwhelmed by traffic, will not stress more than 1 CPU core. Its memory consumption is bounded so, in absence of bugs, it will not stress system memory either. It can itself be rendered unresponsive, of course, by too much traffic but, considering that it probably isn't a vital service for anyone, this isn't something that would excite any attacker. Possible bugs change this picture, however. If the network process is hijacked, even if mitigations for the 1st concern prevent further system penetration, the attacker can still make the network process consume too much CPU and memory. You can try to mitigate against this possibility by limiting daemon CPU and memory usage via [cgroups](https://www.redhat.com/en/blog/cgroups-part-four) or other mechanisms. However, a much simpler solution to these issues is the same as the above - never expose the daemon to the open internet.

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

