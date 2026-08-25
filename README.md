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

### 🔋 Battery Icons
- Colored battery level icons depending on charge:
  - <img src="https://img.shields.io/badge/-100%25-green?style=flat-square"> → Full (>75%)
  - <img src="https://img.shields.io/badge/-75%25-yellowgreen?style=flat-square"> → Good (>50%)
  - <img src="https://img.shields.io/badge/-50%25-orange?style=flat-square"> → Medium (>25%)
  - <img src="https://img.shields.io/badge/-25%25-red?style=flat-square"> → Low (>5%)
  - <img src="https://img.shields.io/badge/-Empty-red?style=flat-square"> → Critical (≤5%)
  - <img src="https://img.shields.io/badge/-Charging-cyan?style=flat-square"> → Charging

- **Battery Icon Pack selector** in **START > UI SETTINGS > BATTERY ICON**:
  - `Default` — colored icons (green/orange/red/cyan)
  - `Power Pulse` — power-style icons
  - `Hearts` — heart-shaped icons
  - `Stock` — original white icons from christianhaitian

- **Network Icon Pack selector** in **START > UI SETTINGS > NETWORK ICON**:
  - `Default` — standard WiFi & Bluetooth icons
  - `Mario` — Mario themed icons
  - `Pokemon` — Pokémon themed icons
  - `Solstice` — Solstice themed icons
  - `Zelda` — Zelda themed icons

### 📶 5-state WiFi icon
- <img src="https://img.shields.io/badge/-WiFi-red?style=flat-square&logo=wifi&logoColor=white"> → Disabled / rfkill blocked
- <img src="https://img.shields.io/badge/-WiFi-orange?style=flat-square&logo=wifi&logoColor=white"> → Interface up, no IP
- <img src="https://img.shields.io/badge/-WiFi-green?style=flat-square&logo=wifi&logoColor=white"> → Connected
- <img src="https://img.shields.io/badge/-Sharing-gold?style=flat-square&logo=gitbook&logoColor=white"> → Active client connected to a sharing service
- <img src="https://img.shields.io/badge/-Active-cyan?style=flat-square&logo=cloudup&logoColor=white"> → Sharing services running (SSH / Samba / Filebrowser)

### 🔵 3-state Bluetooth icon
- <img src="https://img.shields.io/badge/-Bluetooth-red?style=flat-square&logo=bluetooth&logoColor=white"> → Disabled / rfkill blocked
- <img src="https://img.shields.io/badge/-Bluetooth-orange?style=flat-square&logo=bluetooth&logoColor=white"> → Active, no device connected
- <img src="https://img.shields.io/badge/-Bluetooth-blue?style=flat-square&logo=bluetooth&logoColor=white"> → Device connected

- Toggle switches for WiFi and Bluetooth icons in **START > UI SETTINGS**
- Instant reactivity — icon updates **as soon as the state changes** (udev + NetworkManager dispatcher)
- Background daemon (`es-status-daemon`) for polling every 5 seconds

---

### 🖥️ Display Settings
- **Gamma slider** — adjust screen gamma (0.4 → 1.8) in real time via **START > DISPLAY SETTINGS**
- **Mipmap rendering fix** — battery and network icons are now sharp at all sizes
- **Distro Version** — click to check for updates directly from the main menu
- **BatteryPlus calibration** — detection path fixed for dArkOS EN (`/home/ark/.config`)

### 📅 Date & Time (by djparentx)
- Real-time clock display in **START > ADVANCED SETTINGS > DATE & TIME**
- Set date, time and timezone directly from ES

### ⚡ Performance Settings (by djparentx)
New menu — **START > PERFORMANCE SETTINGS**:

| Entry | Description |
|-------|-------------|
| **CPU Temp** | Real-time CPU temperature display |
| **CPU Cores** | Enable/disable CPU cores on the fly (1-4) |
| **CPU Governor** | Switch between performance/ondemand/schedutil/powersave |
| **GPU Governor** | GPU frequency governor |
| **Global Performance** | Quick preset for the whole device |

### 🖥️ Hostname Editor (by djparentx)
- Edit the device hostname directly from ES — **START > NETWORK SETTINGS > HOSTNAME**
- Changes applied immediately with reboot notice

---

### 🌐 Network Settings menu
New menu between **UI Settings** and **Sound Settings** — **START > NETWORK SETTINGS**:

| Entry | Description |
|-------|-------------|
| **Hostname** | Displayed only when SSH or Samba is active |
| **IP Address** | Displayed only when WiFi is connected |
| **Wi-Fi Manager** | Launches `/opt/system/Wi-Fi Manager.sh` |
| **Bluetooth Manager** | Launches `/opt/system/BT Manager.sh` |
| **Samba Sharing** | Toggle Samba on/off instantly |
| **Samba On Boot** | Enable/disable Samba at startup |
| **SSH Sharing** | Toggle SSH on/off instantly |
| **SSH On Boot** | Enable/disable SSH at startup |

---

### 🔋 Battery Settings menu
New menu — **START > BATTERY SETTINGS**:

| Entry | Description |
|-------|-------------|
| **BatteryPlus Status** | Shows daemon active/inactive |
| **Calibration** | Shows calibration state |
| **Battery Level** | Current % from `/tmp/battery.percent` |
| **BatteryPlus Daemon** | Toggle daemon on/off |
| **BatteryPlus Mode** | Switch between `voltage` and `pmic` mode |
| **Reset Calibration** | Delete learned voltage anchors |

Powered by [knubat/BatteryPlus](https://github.com/Mikhailzrick/knubat.components) — a voltage-based battery percentage daemon for RK3326 handhelds.

---

### 📡 Remote Services (by djparentx)
Full remote access management integrated directly in ES:

**One-click toggle** that simultaneously starts/stops:
- SSH server
- Samba file sharing (with optional `/roms2` share)
- Filebrowser (web-based file manager on port 80)
- NetworkManager-wait-online (for NTP sync on connect)

**Additional options:**
- **Samba Root Access** — switch between default and root Samba config
- **Remote Services Auto-start** — enable/disable remote services at boot via `remote-autostart.service`
- **WiFi Monitor** — background service (`wifi_monitor.service`) for WiFi stability
- **WiFi Helpers** — `wifi_enable.sh` / `wifi_disable.sh` scripts
- Optimized Samba configuration with default template (`smb.conf.default`)
- NetworkManager tuning (disable IPv6, background scan, kernel buffer sizes)
- PSK flags fix for persistent WiFi connections

**Gamma slider** — adjust screen gamma (0.4 → 1.8) in real time via **START > DISPLAY SETTINGS**
- **Mipmap rendering fix** — battery and network icons are now sharp at all sizes
- **Distro Version** — click to check for updates directly from the main menu
- **BatteryPlus calibration** — detection path fixed for dArkOS EN (`/home/ark/.config`)

---

### 🌍 Translations
All new strings translated into **17 languages**:
`br` `de` `el` `es` `fr` `it` `ja` `ko` `pl` `pt` `ru` `sv` `ua` `uk` `vi` `zh-CN` `zh-TW`

---

### 🔄 Auto-build
The workflow builds automatically on every push using:
- `aarch64-linux-gnu-g++` cross-compiler
- Official **Mali RK bifrost G31** libraries (not Mesa)
- `-O3 -march=armv8-a+crc -mtune=cortex-a35 -ffast-math`
- ScreenScraper credentials injected via GitHub Secrets

---

## 📋 Requirements

- R36S running **dArkOS EN**
- No internet connection required — everything is included in the zip
- `Wi-Fi Manager.sh` and `BT Manager.sh` in `/opt/system/` for network manager entries

---

## 🚀 Installation

1. Download the latest **`emulationstation-roms-tools`** zip from [GitHub Actions](https://github.com/Jason3x/EmulationStation-fcamod-dArkOS-EN/actions)
2. Extract and copy all contents to: `roms/tools/`
3. Launch `install-es.sh` from the **Tools** section on your device
4. Select **Install ES-dArkOS-EN** — the installer will:
   - Backup the original ES binary
   - Install the new binary + all resources (icons, locales, battery packs, splash)
   - Install and start `es-status-daemon`
   - Install `BatteryPlus` and enable the service
   - Apply launch optimizations

---

## 🙏 Thanks

- [christianhaitian](https://github.com/christianhaitian) for the base EmulationStation fork
- [djparentx](https://github.com/djparentx) for dArkOS EN, the R36S ecosystem and major contributions (gamma, WiFi helpers, network improvements)
- [lcdyk0517](https://github.com/lcdyk0517) for key latency improvements
- [Mikhailzrick](https://github.com/Mikhailzrick) for BatteryPlus

---

## ☕ Support the project

[![Ko-fi](https://img.shields.io/badge/☕_Buy_me_a_coffee-jason3x-red?style=for-the-badge)](https://ko-fi.com/jason3x)
