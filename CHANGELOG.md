# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),

## Unreleased

### Fixed
- Man pages now correctly describe default log destinations

### Changed
- Updated 3rd party dependencies
- Made log messages more uniform and informative.
- Replaced single WSDDN_PREFER_SYSTEM CMake setting with per-dependency WSDDN_PREFER_SYSTEM_&lt;UPPERCASE_DEP_NAME&gt; ones

## [1.21] - 2025-08-04

### Added
- A binary package is now available for OpenBSD. Look for `wsddn-x.y-OpenBSD-amd64.tgz` under Releases
- Configuration file samples for OpenBSD have been provided under `config/openbsd` (h/t Ron Taylor)

### Fixed
- Samba config detection now works properly when `samba` tool is not present. Detection is also
  no longer depends on `whereis` tool. This is particularly an issue on OpenBSD but can be a problem 
  on other platforms too (#17) 
- OpenBSD: UDP writes blocked by firewall no longer stop all processing on an 
  interface (#18)
- OpenBSD and potentially other less common BSD variants: `wsddn` now properly handles
  older BSD behavior where reading from multicast IPv4 sockets surfaces packets sent to
  all interfaces, not the one the socket is restricted to (#19)
- OpenBSD and NetBSD: automatic daemon user creation now works properly. The default expected daemon user name
  is now "_wsddn" on both systems.
- OpenSUSE: RPM package now installs cleanly without warning about missing `firewalld-filesystem` dependency
- CMake build: build is now correctly reconfigures itself when `sys_config.h.in` changes 

### Changed
- To optimize network traffic `wsddn` now sends resolving address directly in the WSD Hello message
  rather than waiting for a call from Windows. This is similar to what `wsdd` does. There appears to
  be no real security benefit in not doing so and it reduces startup traffic significantly.
- FreeBSD config files now live under `config/freebsd` instead of `config/bsd` (all BSDs are not the
  same unfortunately)
- Arch AUR: package build no longer requires network access during build step

## [1.20] - 2025-07-19

### Fixed
- CMake build break when building a clean tree

## [1.19] - 2025-06-28

### Changed
- Updated 3rd party dependencies

## [1.18] - 2025-03-27

### Fixed
- macOS: "Allow in the background" settings now show "wsdd-native" rather than the author name (#16)

### Changed
- Updated 3rd party dependencies

## [1.17] - 2025-03-09

### Changed
- `libuuid` dependency has been replaced with `modern-uuid`
- Updated 3rd party dependencies

## [1.16] - 2025-01-12

### Added
- `--source-port` command line argument and `source-port` config file option to set the source port for 
  multicast messages for better firewall interoperability.
- Configuration files for `firewalld`. These are now also delivered and used in RPM binary packages.

### Changed
- Updated 3rd party dependencies
- Firewall configuration files now live under `config/firewalls`

## [1.15] - 2024-10-03

### Changed
- Updated 3rd party dependencies

## [1.14] - 2024-05-29

### Fixed
- Removed superfluous/misleading warning log messages (#10)

## [1.13] - 2024-05-16

### Added
- On Linux KSMBD configuration is now auto-detected if Samba configuration is not present.
- Binary packages are now available for Arch Linux and Alpine

### Fixed
- Daemon Pidfile now has correct owner

## [1.12] - 2024-05-06

### Added
- Debian binary packages no longer require `systemd` presence. They can now be used on non-`systemd` based distributions as well as Docker. SysVInit scripts are now provided and "do the right thing" on `systemd` and non-`systemd` systems.
- Support for NetBSD
- Support for illumos

## [1.10] - 2024-04-30

### Added
- Ability to supply custom metadata to Windows. This allows exposing the computer running **wsdd-native** as something other than an SMB server in Windows Explorer. For more details see [this page](config/metadata/README.md)
- Support for Alpine Linux and musl libc
- Support for OpenBSD

### Fixed
- Crash when looking up Samba configuration and `whereis` tool is not present on the host

## [1.6] - 2023-07-29

### Added
- Added application profile for UFW on Debian/Ubuntu distribution
- Added `armhf` distribution for Debian/Ubuntu

### Changed
- Updated 3rd party dependencies

## [1.5] - 2023-07-21

### Fixed:
- macOS: Hopefully final fix for #4: `_wsddn` user is reassigned to staff group on OS update 

### Changed:
- Replaced ad-hoc calls to various Posix-y APIs with [ptl](https://github.com/gershnik/ptl)

## [1.4] - 2023-06-16

### Fixed:
- macOS: Corrected `_wsddn` group definition so it is no longer removed from `_wsddn` user on macOS upgrade and no longer shown as available user group in Settings.
- macOS: Made the macOS warning about startup software say "wsddn.app" rather than "Eugene Gershnik" (my developer account name).

## [1.3] - 2023-05-27

### Added
- macOS: Macports distribution is now available

### Fixed
- macOS: child process now logs with correct subsystem and category

### Changed
- macOS: logging to OS log is now the default behavior
- macOS: bundle identifier is now configurable at build time

## [1.2] - 2023-05-21

### Fixed
- APT installer no longer creates bogus `/debian` directory
- Mac `.pkg` installer now correctly includes both x64 and arm64 architectures.

### Changed
- Updated `{fmt}` library dependency 
- It is now possible to specify version externally when building `wsddn` by setting `WSDDN_VERSION` CMake variable.

## [1.1] - 2023-05-19

### Added
- macOS: Allow logging to system log instead of log file. See `--log-os-log` command line option and `log-os-log` config file setting.

### Changed
- Updated all dependencies to latest versions as of 2023-05-18

### Fixed
- All platforms: `wsddn --version` always reporting "0.1"
- macOS: application info dictionary version always set to "0.1"
- Fixed errors when compiling under GCC 13


## [1.0] - 2022-07-25
### Added 
- First official release

## [0.6] - 2022-07-24
### Added
- Initial version


[0.6]: https://github.com/gershnik/wsdd-native/releases/v0.6
[1.0]: https://github.com/gershnik/wsdd-native/releases/v1.0
[1.1]: https://github.com/gershnik/wsdd-native/releases/v1.1
[1.2]: https://github.com/gershnik/wsdd-native/releases/v1.2
[1.3]: https://github.com/gershnik/wsdd-native/releases/v1.3
[1.4]: https://github.com/gershnik/wsdd-native/releases/v1.4
[1.5]: https://github.com/gershnik/wsdd-native/releases/v1.5
[1.6]: https://github.com/gershnik/wsdd-native/releases/v1.6
[1.7]: https://github.com/gershnik/wsdd-native/releases/v1.7
[1.8]: https://github.com/gershnik/wsdd-native/releases/v1.8
[1.9]: https://github.com/gershnik/wsdd-native/releases/v1.9
[1.10]: https://github.com/gershnik/wsdd-native/releases/v1.10
[1.12]: https://github.com/gershnik/wsdd-native/releases/v1.12
[1.13]: https://github.com/gershnik/wsdd-native/releases/v1.13
[1.14]: https://github.com/gershnik/wsdd-native/releases/v1.14
[1.15]: https://github.com/gershnik/wsdd-native/releases/v1.15
[1.16]: https://github.com/gershnik/wsdd-native/releases/v1.16
[1.17]: https://github.com/gershnik/wsdd-native/releases/v1.17
[1.18]: https://github.com/gershnik/wsdd-native/releases/v1.18
[1.19]: https://github.com/gershnik/wsdd-native/releases/v1.19
[1.20]: https://github.com/gershnik/wsdd-native/releases/v1.20
[1.21]: https://github.com/gershnik/wsdd-native/releases/v1.21
