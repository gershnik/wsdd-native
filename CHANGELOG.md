# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),

## Unreleased

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