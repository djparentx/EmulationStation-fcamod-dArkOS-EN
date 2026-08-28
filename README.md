# EmulationStation fcamod — dArkOS EN Edition

![Platform](https://img.shields.io/badge/Platform-R36S-blue)
![OS](https://img.shields.io/badge/OS-dArkOS%20EN-green)
![Shell](https://img.shields.io/badge/Bash-Script-yellow)
![License](https://img.shields.io/badge/License-Free-lightgrey)
![Build](https://img.shields.io/github/actions/workflow/status/Jason3x/EmulationStation-fcamod-dArkOS-EN/build.yml?branch=feature%2Fwifi-bt-icons-network-menu&label=Build)

A custom fork of [christianhaitian/EmulationStation-fcamod](https://github.com/christianhaitian/EmulationStation-fcamod) (branch `351v`) targeting **dArkOS EN** on the **R36S** handheld.
Built automatically via GitHub Actions using the official Mali RK3326 libraries — no cross-compilation issues, no black screen.

---

## ✨ What's new compared to upstream

### 🔋 Battery icon

- Battery level and charging state shown as a small icon in the status bar, with an optional percentage readout.
- **Colored by charge level** (`Default` pack):
  - <img src="https://img.shields.io/badge/-Full%20%2F%20Good%20%2F%20Medium-66c166?style=flat-square"> `> 25%`
  - <img src="https://img.shields.io/badge/-Low-ff4848?style=flat-square"> `≤ 25%`
  - <img src="https://img.shields.io/badge/-Charging-f7ec26?style=flat-square"> while plugged in
- **Icon pack selector** — **START > UI SETTINGS > BATTERY ICON**:

  | Pack | Look |
  |------|------|
  | `Default` | Simple green battery icons |
  | `Colorful` | Graduated colors by level (full → good → medium → low → charging) |
  | `Hearts` | Heart-shaped icons |
  | `Stock` | Original monochrome icons from christianhaitian |

- Powered by [knubat/BatteryPlus](https://github.com/Mikhailzrick/knubat.components) — a voltage-based percentage daemon for RK3326 handhelds, with its own settings menu (see below).

### 📶🔵 WiFi & Bluetooth icons

- Live status icons in the status bar, each with an **off / active / connected** state (WiFi also has **sharing** and **service** states for SSH/Samba/Filebrowser).
- Icon appearance updates automatically every 5 seconds via a background daemon (`es-status-daemon`) — no polling from ES itself.
- Toggle each icon independently in **START > UI SETTINGS**.
- **Icon pack selector** — **START > UI SETTINGS > NETWORK ICON**: `Default`, `Mario`, `Pokémon`, `Solstice`, `Zelda`.
- Switching either the **battery** or the **network/Bluetooth** icon pack applies **instantly**, with no EmulationStation restart required.
- Status bar spacing is unified: the gap between the WiFi, Bluetooth and battery icons, and between the battery icon and its percentage, all match.

### 🖥️ Display Settings
- **Gamma slider** — adjust screen gamma (0.4 → 1.8) in real time, in **START > DISPLAY SETTINGS**.
- **Distro Version** — click to check for dArkOS EN updates directly from the main menu.

### 📅 Date & Time (by djparentx)
- Real-time clock in **START > ADVANCED SETTINGS > DATE & TIME**.
- Set date, time and timezone directly from ES.

### ⚡ Performance Settings (by djparentx)
New menu — **START > PERFORMANCE SETTINGS**:

| Entry | Description |
|-------|-------------|
| **CPU Temp** | Real-time CPU temperature |
| **CPU Cores** | Enable/disable CPU cores on the fly (1–4) |
| **CPU Governor** | performance / ondemand / schedutil / powersave |
| **CPU Max Freq** | Set CPU clock speed |
| **GPU Governor** | GPU frequency governor |
| **GPU Max Freq** | Set GPU clock speed |
| **RAM Max Freq** | Set RAM clock speed |
| **ZRAM Enable** | Enable disable ZRAM, persists after reboot |
| **ZRAM Size** | Set ZRAM size |
| **ZRAM Compression** | Choose ZRAM compression |

### 🌐 Network Settings menu
New menu between **UI Settings** and **Sound Settings** — **START > NETWORK SETTINGS**:
Wi-Fi Manager is fully integrated into the menu and OS.

| Entry | Description |
|-------|-------------|
| **Current Network** | Shows current connected network |
| **Hostname** | Editable; shown only when Remote Services is active |
| **IP Address** | Shown only when WiFi is connected |
| **Gateway** | Shown only when WiFi is connected |
| **DNS** | Shown only when WiFi is connected |
| **Wi-Fi Enabled** | Enables/disables wifi |
| **Scan Wifi Networks** | Scans for open networks |
| **Activate Existing Connection** | Connects to saved networks |
| **Delete Existing Connection** | Delete a saved network |
| **Remote Services** | Enable/disable ssh, samba, and filebrowser sharing in one click |
| **Remote Services Auto-start** | Start remote services at boot |
| **Root Samba Access** | Enable/disable root access for samba, toggle to add SD2 if available |
| **Wifi Monitor Service** | Enable/disable wifi monitor service |
| **Bluetooth Manager** | Launches `/opt/system/BT Manager.sh` |

### 🔋 Battery Settings menu
New menu — **START > BATTERY SETTINGS**:

| Entry | Description |
|-------|-------------|
| **BatteryPlus Status** | Daemon active/inactive |
| **Calibration** | Current calibration state |
| **Battery Level** | Live % from `/tmp/battery.percent` |
| **BatteryPlus Enabled** | Toggle on/off |
| **BatteryPlus Mode** | `voltage` or `pmic` |
| **Reset Calibration** | Delete learned voltage anchors |

### 📡 Remote Services (by djparentx)
One-click toggle that starts/stops **SSH**, **Samba** (with optional `/roms2` share) and **Filebrowser** (web file manager on port 80) together, plus:
- **Samba Root Access** — default vs. root Samba config
- **Auto-start at boot** for all remote services
- **WiFi Monitor** — background service for connection stability
- Tuned NetworkManager config (IPv6 off, background scan, buffer sizes) and a PSK fix for persistent WiFi connections

### 🌍 Translations
All new strings translated into **17 languages**:
`br` `de` `el` `es` `fr` `it` `ja` `ko` `pl` `pt` `ru` `sv` `ua` `uk` `vi` `zh-CN` `zh-TW`

### 🔄 Auto-build
Every push builds automatically via GitHub Actions:
- `aarch64-linux-gnu-g++` cross-compiler
- Official **Mali RK bifrost G31** libraries (not Mesa)
- `-O3 -march=armv8-a+crc -mtune=cortex-a35 -ffast-math`
- ScreenScraper credentials injected via GitHub Secrets

---

## 📋 Requirements

- R36S running **dArkOSen**
- No internet connection required — everything is included in the zip
- `Update.sh` and `BT Manager.sh` in `/opt/system/` for the network manager entries

---

## 🚀 Installation

1. Download the latest **`emulationstation-roms-tools`** zip from [GitHub Actions](https://github.com/Jason3x/EmulationStation-fcamod-dArkOS-EN/actions)
2. Extract and copy all contents to: `roms/tools/`
3. Launch `install-es.sh` from the **Tools** section on your device
4. Select **Install ES-dArkOS-EN** — the installer will:
   - Back up the original ES binary
   - Install the new binary + all resources (icons, locales, battery/network packs, splash)
   - Install and start `es-status-daemon`
   - Install `BatteryPlus` and enable the service
   - Apply launch optimizations

---

## 🙏 Thanks

- [christianhaitian](https://github.com/christianhaitian) for the base EmulationStation fork
- [djparentx](https://github.com/djparentx) for dArkOS EN, the R36S ecosystem and major contributions (gamma slider, WiFi helpers, Remote Services, Performance Settings, Date & Time)
- [lcdyk0517](https://github.com/lcdyk0517) for key latency improvements
- [Mikhailzrick](https://github.com/Mikhailzrick) for BatteryPlus

---

## ☕ Support the project

[![Ko-fi](https://img.shields.io/badge/☕_Buy_me_a_coffee-jason3x-red?style=for-the-badge)](https://ko-fi.com/jason3x)
