#!/bin/bash

if [ "$(id -u)" -ne 0 ]; then
    exec sudo -- "$0" "$@"
fi

unzip -X -o /opt/system/Tools/es-helpers.zip -d /

# permissions
chmod 644 /etc/samba/smb.conf
chmod 644 /samba/smb.conf.default
chmod 644 /etc/samba/smb.conf.root
chmod +x "/usr/local/bin/wifi_monitor.sh"
chmod +x "/usr/local/bin/wifi_enable.sh"
chmod +x "/usr/local/bin/wifi_disable.sh"
chmod +x "/usr/local/bin/boot_volume.sh"
chmod +x "/usr/local/bin/batt_life_warning.py.red"
chmod +x "/usr/local/bin/batt_life_warning.py.green"
chmod +x "/usr/local/bin/batt_life_warning.py"
chmod +x "/usr/local/bin/fix_power_led.red"
chmod +x "/usr/local/bin/fix_power_led.green"
chmod +x "/usr/local/bin/fix_power_led"
chmod +x "/etc/NetworkManager/dispatcher.d/99-disable-bgscan.sh"
chmod +x "/etc/NetworkManager/dispatcher.d/99-disable-ipv6.sh"
chown ark:ark /etc/samba/smb.conf
chown ark:ark /etc/samba/smb.conf.default
chown ark:ark /etc/samba/smb.conf.root
systemctl daemon-reload
systemctl enable fix-power-led

# cleanup
rm -f /etc/samba/smb.conf.bak

# disable netbios name in samba
sed -i 's/^   netbios name = /#   netbios name = /' /etc/samba/smb.conf

# reload network manager
nmcli general reload

# --- set kernel network buffer sizes ---
if ! grep -q "rmem_max = 524288" /etc/sysctl.conf 2>/dev/null; then
    echo 'net.core.rmem_max = 524288' >> /etc/sysctl.conf
    echo 'net.core.wmem_max = 524288' >> /etc/sysctl.conf
    sysctl -p > /dev/null 2>&1
fi

# --- set psk flags to 0 ---
nmcli -t -f NAME con show | while read -r name; do
    nmcli con modify "$name" wifi-sec.psk-flags 0 2>/dev/null || true
    nmcli con modify "$name" 802-11-wireless.bgscan "" 2>/dev/null || true
done

# --- update new state flag ---
cp -f /tmp/wifi_manager_state /var/cache/wifi_manager_state