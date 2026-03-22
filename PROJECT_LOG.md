# Apple TV Remote — Project Log

## 2026-03-14 — v1 Code Complete

### Design & Planning
- Brainstormed requirements: BLE HID primary, IR secondary (Phase 2), Siri Remote emulation (Phase 3)
- Targeting all Apple TV generations, Momentum custom firmware, App Catalog submission
- Chose ViewDispatcher pattern over SceneManager (only 2 views, SceneManager is overkill)
- Design spec written, reviewed by automated spec reviewer, all issues resolved
- Implementation plan: 13 tasks across 4 chunks, reviewed and approved

### Implementation
- **Chunk 1 (scaffold):** App manifest, shared header with App/BleState/ViewId types, entry point with full BLE HID lifecycle (profile switch, advertising, bond storage, cleanup), settings persistence via FlipperFormat
- **Chunk 2 (remote view):** Draw callback with D-pad visual + BLE status, navigation via HID keyboard arrow keys (press/release for accurate key-down/key-up), volume control via long-press Up/Down with 400ms periodic timer (~2.5/sec), view leave callback to clean up HID state
- **Chunk 3 (settings/docs):** Settings submenu with Forget Pairing (proper service-layer BLE re-init), 10x10 1-bit app icon, SPEC.md, PROJECT_LOG.md
- **Chunk 4 (ship):** Include graph verified clean, GPL-2.0 license added, public GitHub repo created

### Code Review Findings (all resolved)
- Ok button: deferred HID RETURN from InputTypePress to InputTypeShort to avoid leaking RETURN when long-pressing Ok to open settings
- View leave callback added to stop volume timer and release all HID keys on view switch
- BLE status callback: documented intentional simplification (BtStatusOff collapsed into Advertising state)
- ForgetPairing: switched from direct `furi_hal_bt_start_advertising()` to `bt_set_profile()` for clean re-init after key deletion
- Settings save: added FURI_LOG_E on write failure

### Decisions
- **Open source (GPL-2.0):** Flipper ecosystem is GPL, App Catalog expects source access, no viable commercialization path, community benefits outweigh closed-source
- **Device name not yet configurable in UI:** FlipperFormat field ready, but Flipper text input widget adds too much complexity for v1. Deferred to v1.1.
- **No advertising timeout or reconnection retry logic in v1:** BLE stack handles basic reconnection. Explicit retry deferred to v1.1.

### Current State
- v1 code complete (15 commits)
- GitHub: https://github.com/1507-systems/flipper-apple-tv-remote (public)
- **NOT YET BUILT OR TESTED** — requires Momentum firmware build environment and real Apple TV hardware

### Next Steps
1. Clone Momentum-Firmware, symlink app into `applications_user/`
2. Build: `./fbt fap_apple_tv_remote`
3. Deploy `.fap` to Flipper via qFlipper or SD card
4. Hardware test: pairing, navigation, volume, settings, edge cases
5. Fix any build/runtime issues
6. Run `/full-audit` before declaring production-ready
7. Submit to Momentum App Catalog

---

## 2026-03-22 — Code Audit + Build/Catalog Documentation

### What Was Done

Performed a full CLI code audit of all source files. Created README.md (required for App Catalog submission per spec). Documented build instructions and App Catalog submission steps.

### Audit Results — Code Quality

All source files reviewed: apple_tv_remote.h, apple_tv_remote.c, views/remote_view.c, views/remote_view.h, views/settings_view.c, views/settings_view.h, settings/settings_storage.c, settings/settings_storage.h, application.fam.

No functionality issues found. Specific findings:

- FlipperFormat resource management (settings_storage.c): flipper_format_file_close and flipper_format_free are called unconditionally after flipper_format_file_open_existing — correct. The Flipper SDK treats close on an un-opened file as a no-op.
- BLE state simplification: BtStatusOff collapses into BleStateAdvertising (shows Searching... when radio is off). Intentional, documented in code, acceptable for v1.
- Input handling for Ok button: Press deferred to InputTypeShort to avoid leaking RETURN on long-press to settings. Correct and deliberate.
- View leave callback: Properly stops volume timer and releases all HID keys on any view switch. No key-stuck bugs possible.
- ForgetPairing re-init: Uses bt_set_profile() (service layer) rather than raw furi_hal_bt_start_advertising() — correct pattern for post-bond-delete re-init.
- Stack size (2 * 1024): Matches the standard for simple BLE HID apps in Momentum. Acceptable.
- settings_view.c depends on remote_view.h: Needed for remote_view_set_ble_state() after ForgetPairing. Not circular. Fine.

No security issues. No user-controlled strings passed to unsafe functions; strlcpy used with explicit buffer sizes throughout.

### Audit Results — Documentation

- Missing README.md: Required for App Catalog submission. Created README.md with usage, button mapping, pairing flow, build instructions, and App Catalog submission steps.
- application.fam: All required fields present. fap_author is the GitHub handle; acceptable for catalog.
- SPEC.md: Present, links to full design doc.
- LICENSE: GPL-2.0 present.
- application.fam icon path matches actual file location.

### Blockers — Hardware-Dependent Steps (Cannot Complete from CLI)

The following steps require a physical Flipper Zero running Momentum firmware and an Apple TV:

1. Build verification: ./fbt fap_apple_tv_remote must be run inside a Momentum-Firmware clone. No build environment available on this machine.
2. Hardware testing: Full checklist in README.md under Hardware testing required before submission. Until this passes, app is not ready for catalog submission.
3. App Catalog PR: Requires a tested, built .fap and a specific commit_sha of the release tag. Blocked on hardware test.

### Current State

- Code: complete, audit-clean (zero functionality issues, zero security issues)
- README: added (was missing, now present)
- Build + hardware test: pending (requires Momentum firmware environment + Flipper Zero hardware)
- App Catalog submission: pending hardware test

### Next Steps (Revised)

1. Set up Momentum firmware build environment (Linux/macOS/WSL)
2. ln -s /path/to/flipper-apple-tv-remote applications_user/apple_tv_remote
3. ./fbt fap_apple_tv_remote — fix any build errors
4. Deploy to Flipper, run hardware test checklist in README
5. Tag release: git tag v1.0.0 and git push origin v1.0.0
6. Submit to Momentum App Catalog (PR to Next-Flip/flipper-application-catalog)
