#!/bin/bash

#------------------------------------#
#   ES dArkOS-EN Installer - R36S   #
#            By Jason                #
#------------------------------------#

# --- Vérification des privilèges root ---
if [ "$(id -u)" -ne 0 ]; then
    exec sudo -E "$0" "$@"
fi

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
CURR_TTY="/dev/tty1"
BACKTITLE="ES dArkOS-EN Installer R36S - By Jason -"

ES_INSTALL_PATH="/usr/bin/emulationstation/emulationstation"
ES_RESOURCES_PATH="/usr/bin/emulationstation/resources"
ES_BACKUP="/root/es_original_backup"
BACKUP_FLAG="/root/.es_installer_backup_done"

# Fichiers sources dans roms/tools
ES_BINARY="$SCRIPT_DIR/emulationstation"
ES_RESOURCES_DIR="$SCRIPT_DIR/resources"
ES_SCRIPTS_DIR="$SCRIPT_DIR/scripts"

# --- Préparation affichage ---
printf "\033c" > "$CURR_TTY"
printf "\e[?25l" > "$CURR_TTY"
dialog --clear

# --- Sélection de la police ---
if [[ ! -e "/dev/input/by-path/platform-odroidgo2-joypad-event-joystick" ]]; then
    setfont /usr/share/consolefonts/Lat7-TerminusBold22x11.psf.gz
else
    setfont /usr/share/consolefonts/Lat7-Terminus16.psf.gz
fi

pkill -9 -f gptokeyb || true
pkill -9 -f osk.py    || true

# --- Animation splash ---
printf "\033c" > "$CURR_TTY"
for i in {1..2}; do
    printf "Starting ES dArkOS-EN Installer...\nPlease wait." > "$CURR_TTY"
    sleep 0.6
    printf "\033c" > "$CURR_TTY"
    sleep 0.4
done

# --- Message de bienvenue ---
printf "\033c" > "$CURR_TTY"
printf "\n\n" > "$CURR_TTY"
printf "      ========================================\n" > "$CURR_TTY"
printf "        Welcome to ES dArkOS-EN Installer     \n" > "$CURR_TTY"
printf "                    By Jason                  \n" > "$CURR_TTY"
printf "      ========================================\n" > "$CURR_TTY"
sleep 2
printf "\033c" > "$CURR_TTY"

# --- Progression fluide ---
smooth_progress() {
    local msg=$1
    local delay=$2
    local start_val=$3
    local end_val=$4
    for ((i=start_val; i<=end_val; i++)); do
        echo "$i"
        echo "XXX"; echo -e "$msg"; echo "XXX"
        sleep "$delay"
    done
}

# --- Vérification des fichiers sources ---
check_sources() {
    if [ ! -f "$ES_BINARY" ]; then
        dialog --backtitle "$BACKTITLE" --title "Error" \
            --msgbox "\nES binary not found:\n$ES_BINARY\n\nPlease check roms/tools content." 10 55 > "$CURR_TTY"
        return 1
    fi
    return 0
}

# --- Sauvegarde de l'ES ---
backup_es_if_needed() {
    if [ ! -f "$BACKUP_FLAG" ]; then
        if [ -f "$ES_INSTALL_PATH" ]; then
            cp "$ES_INSTALL_PATH" "$ES_BACKUP"
            touch "$BACKUP_FLAG"
        fi
    fi
}

# --- Installation des SVG ---
install_svgs() {
    if [ -d "$ES_RESOURCES_DIR" ]; then
        cp -r "$ES_RESOURCES_DIR/." "$ES_RESOURCES_PATH/"
    fi
}

# --- Installation des locales ---
install_locales() {
    if [ -d "$TOOLS_DIR/resources/locale" ]; then
        mkdir -p "$ES_RES/locale"
        cp -r "$TOOLS_DIR/resources/locale/." "$ES_RES/locale/"
    fi
}

# --- Installation BatteryPlus ---
install_batteryplus() {
    if [ -f "$TOOLS_DIR/batteryplus" ]; then
        sudo install -m 755 "$TOOLS_DIR/batteryplus" /usr/bin/batteryplus

        # Service systemd
        sudo tee /etc/systemd/system/batteryplus.service > /dev/null << 'SVCEOF'
[Unit]
Description=BatteryPlus Battery Daemon
After=multi-user.target

[Service]
Type=simple
ExecStart=/usr/bin/batteryplus
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
SVCEOF

        # Config par défaut
        sudo mkdir -p /etc/batteryplus /userdata/system/configs/batteryplus
        if [ ! -f /etc/batteryplus/batteryplus.conf ]; then
            sudo tee /etc/batteryplus/batteryplus.conf > /dev/null << 'CONFEOF'
[Config]
mode=voltage
data_dir=/userdata/system/configs/batteryplus
V_EMPTY_CHG=3400
V_EMPTY_DIS=3250
CONFEOF
        fi

        sudo systemctl daemon-reload
        sudo systemctl enable batteryplus.service
        sudo systemctl restart batteryplus.service
    fi
}

# --- Suppression des SVG ---
remove_svgs() {
    local svgs="bluetooth.svg bluetooth_active.svg bluetooth_off.svg network.svg network_active.svg network_off.svg network_share.svg network_service.svg"
    for svg in $svgs; do
        rm -f "$ES_RESOURCES_PATH/$svg"
    done
}

# --- Optimisations du lancement ---
apply_optimizations() {
    ES_LAUNCH="/usr/bin/emulationstation/emulationstation.sh"

    if [ ! -f "${ES_LAUNCH}.bak" ]; then
        cp "$ES_LAUNCH" "${ES_LAUNCH}.bak"
    fi

    sed -i "/ff400000.gpu.*governor/d" "$ES_LAUNCH"
    sed -i "/policy0.*scaling_governor/d" "$ES_LAUNCH"
    sed -i "/dmc.*governor/d" "$ES_LAUNCH"
    sed -i "/export SDL_VIDEO_DOUBLE_BUFFER/d" "$ES_LAUNCH"

    if ! grep -q "schedutil" "$ES_LAUNCH"; then
        sed -i "s|rm -f /tmp/es-restart /tmp/es-sysrestart /tmp/es-shutdown|echo schedutil | tee /sys/devices/system/cpu/cpufreq/policy0/scaling_governor > /dev/null\n        echo 1200000 | tee /sys/devices/system/cpu/cpufreq/policy0/scaling_max_freq > /dev/null\n        iw dev wlan0 set power_save on 2>/dev/null || true\n        rm -f /tmp/es-restart /tmp/es-sysrestart /tmp/es-shutdown|" "$ES_LAUNCH"
    fi

    if ! grep -q "SDL_RENDER_VSYNC" "$ES_LAUNCH"; then
        sed -i "s|export SDL_ASSERT=\"always_ignore\"|export SDL_ASSERT=\"always_ignore\"\nexport SDL_RENDER_VSYNC=0\nexport SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1|" "$ES_LAUNCH"
    else
        if ! grep -q "SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS" "$ES_LAUNCH"; then
            sed -i "s|export SDL_RENDER_VSYNC=0|export SDL_RENDER_VSYNC=0\nexport SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1|" "$ES_LAUNCH"
        fi
    fi

    # Installer le daemon depuis scripts/
    if [ -f "$ES_SCRIPTS_DIR/install-status-daemon.sh" ]; then
        bash "$ES_SCRIPTS_DIR/install-status-daemon.sh" > /dev/null 2>&1
    fi
}

# --- Suppression des optimisations ---
remove_optimizations() {
    ES_LAUNCH="/usr/bin/emulationstation/emulationstation.sh"
    if [ -f "${ES_LAUNCH}.bak" ]; then
        cp "${ES_LAUNCH}.bak" "$ES_LAUNCH"
        rm -f "${ES_LAUNCH}.bak"
    fi

    systemctl stop es-status-daemon.service 2>/dev/null
    systemctl disable es-status-daemon.service 2>/dev/null
    rm -f /etc/systemd/system/es-status-daemon.service
    rm -f /usr/local/bin/es-status-daemon.sh
    systemctl daemon-reload

    rm -f /etc/NetworkManager/dispatcher.d/99-es-wifi-icon

    for SERVICE in ssh smbd nmbd filebrowser; do
        rm -f "/etc/systemd/system/${SERVICE}.service.d/es-icon.conf"
        rmdir "/etc/systemd/system/${SERVICE}.service.d" 2>/dev/null
    done
    systemctl daemon-reload 2>/dev/null

    rm -f /etc/udev/rules.d/99-es-icons.rules
    udevadm control --reload-rules

    rm -f /tmp/es-wifi-state /tmp/es-bt-state /tmp/es-wifi-changed /tmp/es-bt-changed
}

# --- Installation ES-dArkOS-EN ---
Install_dArkOS_EN() {
    check_sources || return

    dialog --backtitle "$BACKTITLE" --title "Install ES-dArkOS-EN" \
        --yesno "\nInstall ES-dArkOS-EN on your R36S?\n\nThis will install:\n- EmulationStation with WiFi/BT icons\n- Network Settings menu\n- Status daemon\n\nThe device will reboot when done." 13 60 > "$CURR_TTY"
    [ $? -ne 0 ] && return

    (
        smooth_progress "Backing up original ES..." 0.04 0 10
        backup_es_if_needed

        smooth_progress "Installing ES binary..." 0.03 11 40
        install -m 755 -o root -g root "$ES_BINARY" "$ES_INSTALL_PATH"

        smooth_progress "Installing SVG icons..." 0.05 41 65
        install_svgs
        install_locales

        smooth_progress "Installing BatteryPlus..." 0.05 56 75
        install_batteryplus

        smooth_progress "Applying optimizations..." 0.05 76 90
        apply_optimizations

        smooth_progress "Finalizing..." 0.03 91 100
    ) | dialog --backtitle "$BACKTITLE" --title "Install ES-dArkOS-EN" \
        --gauge "\nInstalling, please wait..." 8 60 0 > "$CURR_TTY"

    dialog --backtitle "$BACKTITLE" --title "Install ES-dArkOS-EN" \
        --msgbox "\nES-dArkOS-EN installed successfully!\n\nRebooting R36S..." 8 55 > "$CURR_TTY"

    reboot
}

# --- Restauration de l'ES original ---
Restore_ES() {
    if [ ! -f "$ES_BACKUP" ]; then
        dialog --backtitle "$BACKTITLE" --title "Restore" \
            --msgbox "\nNo backup found.\n\nPlease run an installation first\nto create an automatic backup." 10 55 > "$CURR_TTY"
        return
    fi

    dialog --backtitle "$BACKTITLE" --title "Restore" \
        --yesno "\nRestore the original EmulationStation?\n\nInstalled SVG icons will be removed.\nOptimizations will be undone.\nThe device will reboot when done." 10 55 > "$CURR_TTY"
    [ $? -ne 0 ] && return

    (
        smooth_progress "Removing optimizations..." 0.05 0 30
        remove_optimizations

        smooth_progress "Restoring original ES..." 0.05 31 60
        install -m 755 -o root -g root "$ES_BACKUP" "$ES_INSTALL_PATH"

        smooth_progress "Removing SVG icons..." 0.05 61 90
        remove_svgs

        smooth_progress "Finalizing..." 0.03 91 100
    ) | dialog --backtitle "$BACKTITLE" --title "Restore" \
        --gauge "\nRestoring, please wait..." 8 60 0 > "$CURR_TTY"

    dialog --backtitle "$BACKTITLE" --title "Restore" \
        --msgbox "\nOriginal ES restored successfully!\n\nRebooting R36S..." 8 55 > "$CURR_TTY"

    reboot
}

# --- Quitter ---
Exit_Script() {
    printf "\033c" > "$CURR_TTY"
    printf "\e[?25h" > "$CURR_TTY"
    pkill -f "gptokeyb" || true
    exit 0
}

# --- Menu Principal ---
Main_Menu() {
    while true; do
        if [ -f "$BACKUP_FLAG" ]; then
            BACKUP_STATUS="\Z2Backup found\Zn"
        else
            BACKUP_STATUS="\Z1No backup\Zn"
        fi

        selection=$(dialog --colors --backtitle "$BACKTITLE" --title " MAIN MENU " \
            --cancel-label "Exit" \
            --menu "\nBackup status: $BACKUP_STATUS\n\nSelect an option:" 14 60 3 \
            1 "Install ES-dArkOS-EN" \
            2 "Restore original ES" \
            3 "Exit" 2>&1 > "$CURR_TTY")

        [ $? -ne 0 ] && Exit_Script

        case $selection in
            1) Install_dArkOS_EN ;;
            2) Restore_ES ;;
            3) Exit_Script ;;
        esac
    done
}

# --- Mapping des touches ---
export SDL_GAMECONTROLLERCONFIG_FILE="/opt/inttools/gamecontrollerdb.txt"
/opt/inttools/gptokeyb -1 "$(basename "$0")" -c "/opt/inttools/keys.gptk" > /dev/null 2>&1 &

trap Exit_Script EXIT

# --- Lancement ---
Main_Menu
