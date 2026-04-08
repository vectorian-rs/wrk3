# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] - 2026-04-08

### Changed
- Switch dependencies from FetchContent to shallow git submodules (faster builds, offline support)
- Pin aws-lc to v1.71.0, s2n-tls to v1.7.2
- Skip building unused aws-lc targets (libssl, CLI tools)
- CI uses targeted submodule init instead of recursive clone

### Fixed
- Link failure on Linux CI caused by system OpenSSL poisoning s2n-tls feature probes
- Format specifier bugs for uint64_t and uint32_t values
- Operator precedence bug in URL part allocation
- macOS deployment target mismatch causing LuaJIT linker warnings
- Debug output now goes to stderr
- Added defensive default cases to status enum switches
- Parenthesized MAX_LATENCY macro

### Removed
- Dead print_stats_latency function

### Added
- mise.toml for build task automation

## [0.1.0] - 2026-04-03

### Added
- Initial release — fork of wrk2 with CMake build system
- HTTP benchmarking with constant-throughput, correct-latency measurement
- Lua scripting support
- HdrHistogram latency recording

[0.2.0]: https://github.com/vectorian-rs/wrk3/releases/tag/v0.2.0
[0.1.0]: https://github.com/vectorian-rs/wrk3/releases/tag/v0.1.0
