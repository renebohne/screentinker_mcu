# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Support for server-coordinated power schedules and deep sleep via `X-ST-Power-State` and `X-ST-Sleep-Seconds` headers ([#2](https://github.com/renebohne/screentinker_mcu/pull/2)).
- Serial logging for server power states (`on` vs. `scheduled_off`).

### Changed
- Guarded adaptive sleep backoff logic: when the server requests long sleep intervals ($\ge 1800\,\text{s}$, such as night-time or inactive power schedules), consecutive HTTP 304 responses will no longer clamp the sleep down to 300 s (5 min) or 1800 s (30 min) ([#2](https://github.com/renebohne/screentinker_mcu/pull/2)).

## [1.1.0] - 2024-09-04

### Added
- Seamless 6-digit PIN pairing mechanism with ScreenTinker server.
- Multi-zone and fullscreen layout rendering options (`/api/embedded/render-layout`).
- On-device system menu (hold OK for 1.5 s).
- Bilingual onboarding support (English and German).
- WebSerial installer and captive portal provisioning.
- 8 MB PSRAM in-memory frame cache supporting up to 32 frames.
