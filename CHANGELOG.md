# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),

## Unreleased

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
