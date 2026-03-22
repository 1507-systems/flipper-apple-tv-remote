# Apple TV Remote for Flipper Zero

Control Apple TV via BLE HID from your Flipper Zero. Works with Apple TV 4th gen HD, 4K (all generations), and any Apple TV running tvOS 11+.

## Features

- BLE HID pairing — pairs like a standard Bluetooth keyboard/remote
- D-pad navigation (Up, Down, Left, Right, Select, Menu/Back)
- Volume control via long-press Up/Down (holds ~2.5 increments/sec)
- Bond persistence — auto-reconnects on relaunch
- Forget Pairing option in settings
- Settings saved between launches

## Requirements

- Flipper Zero running **Momentum firmware** (latest stable)
- Apple TV 4th gen HD or later (BLE required)

## Button Mapping

| Flipper Button   | Short Press        | Long Press (>400ms)     |
|------------------|--------------------|-------------------------|
| D-pad Up         | Navigate Up        | Volume Up (repeating)   |
| D-pad Down       | Navigate Down      | Volume Down (repeating) |
| D-pad Left       | Navigate Left      | —                       |
| D-pad Right      | Navigate Right     | —                       |
| OK (center)      | Select             | Open settings menu      |
| Back             | Menu / Back (tvOS) | Exit app                |

## Pairing

1. Launch the app on your Flipper Zero — it will start advertising as "FlipperTV Remote"
2. On Apple TV: Settings → Remotes and Devices → Bluetooth
3. Select "FlipperTV Remote" from the list
4. Pairing completes — the Flipper screen shows "BLE: Connected"

On subsequent launches the Flipper auto-reconnects to the bonded Apple TV.

To pair with a different Apple TV: long-press OK → Settings → Forget Pairing, then repeat the steps above.

## Build Instructions (Momentum firmware)

This app requires the Momentum firmware build environment. It cannot be compiled with standard Flipper Zero Official firmware.

### Prerequisites

- Linux, macOS, or WSL2
- Python 3.8+
- Git

### Steps

```bash
# 1. Clone Momentum firmware
git clone --recursive https://github.com/Next-Flip/Momentum-Firmware.git
cd Momentum-Firmware

# 2. Symlink this app into the user applications directory
ln -s /path/to/flipper-apple-tv-remote applications_user/apple_tv_remote

# 3. Build the FAP (Flipper Application Package)
./fbt fap_apple_tv_remote

# Output: build/f7-firmware-D/.extapps/apple_tv_remote.fap
```

### Deploy to Flipper

**Via qFlipper (USB):**
1. Open qFlipper, connect Flipper via USB
2. Navigate to SD card: `apps/Bluetooth/`
3. Copy `apple_tv_remote.fap` to that folder

**Via SD card:**
1. Copy `apple_tv_remote.fap` to `SD:/apps/Bluetooth/`
2. Insert SD into Flipper

The app appears in: Applications → Bluetooth → Apple TV Remote

## App Catalog Submission

The Momentum App Catalog accepts FAP submissions via pull request to the
[Next-Flip/flipper-application-catalog](https://github.com/Next-Flip/flipper-application-catalog)
repository.

### Submission package checklist

- `application.fam` — present, `appid="apple_tv_remote"`, `fap_category="Bluetooth"`, `fap_version=(1, 0)`
- App icon — `assets/icons/remote_10px.png` (10x10 1-bit PNG)
- README — this file
- License — GPL-2.0 (`LICENSE`)
- Source repo — https://github.com/1507-systems/flipper-apple-tv-remote (public)

### Submission steps

1. Fork `Next-Flip/flipper-application-catalog`
2. Add an entry under `applications/Bluetooth/apple_tv_remote/`:
   ```
   manifest.yml   (see catalog format below)
   ```
3. Open a pull request against the catalog's `main` branch

**`manifest.yml` template:**
```yaml
sourcecode:
  type: git
  location:
    origin: https://github.com/1507-systems/flipper-apple-tv-remote.git
    commit_sha: <commit hash of release tag>
```

4. The catalog maintainers will build the FAP, review it, and merge

### Hardware testing required before submission

The following must be verified on real hardware before submitting to the catalog:

- [ ] BLE advertising starts on app launch
- [ ] Pairs successfully with Apple TV (4th gen HD or later)
- [ ] Navigation D-pad controls work (Up/Down/Left/Right/Select)
- [ ] Short Back sends Menu to tvOS
- [ ] Long Back (>1s) exits the app cleanly, restores Flipper Serial BLE profile
- [ ] Volume Up long-press activates volume mode, repeats ~2.5/sec, stops on release
- [ ] Volume Down same
- [ ] Forget Pairing clears bond and restarts advertising
- [ ] Settings saved and loaded between app launches
- [ ] BLE profile restored after exit (Flipper mobile app still works)

## Project Status

| Phase | Status |
|-------|--------|
| v1 — BLE HID Remote | Code complete, pending hardware test |
| Phase 2 — IR Mode | Not started |
| Phase 3 — Siri Remote Protocol | Exploratory |

## License

GPL-2.0. See [LICENSE](LICENSE).
