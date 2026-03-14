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
