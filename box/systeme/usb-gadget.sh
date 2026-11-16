#!/bin/sh
# Octavio box: lets a computer plugged into the POWER USB-C port see the box as a small network (USB gadget), and
# opens the file transfer page there (http://octavio.local). Run once on the Pi, as root:  sudo sh usb-gadget.sh
# The computer's USB-C port then powers the box: it must give 3 A (most laptops' USB-C ports do), else use a powered
# USB-C hub between them. With the usual power supply in that port, nothing changes.
set -e
CFG=/boot/firmware/config.txt; CMD=/boot/firmware/cmdline.txt
# 1. the Pi 5's USB-C port as a USB device (its OTG controller), an Ethernet gadget (ECM + RNDIS: macOS, Linux, Windows)
grep -q '^dtoverlay=dwc2,dr_mode=peripheral' "$CFG" || printf '\n[all]\ndtoverlay=dwc2,dr_mode=peripheral\n' >> "$CFG"
grep -q 'modules-load=dwc2,g_ether' "$CMD" || sed -i 's/$/ modules-load=dwc2,g_ether/' "$CMD"
# 2. the link: the box is 10.55.0.1 and hands the computer an address (NetworkManager "shared")
nmcli con delete octavio-usb >/dev/null 2>&1 || true
nmcli con add type ethernet ifname usb0 con-name octavio-usb autoconnect yes ipv4.method shared ipv4.addresses 10.55.0.1/24 ipv6.method ignore
# 3. the name octavio.local, and port 80 sent to the page (it runs as the user, on 8080)
hostnamectl set-hostname octavio
apt-get install -y avahi-daemon nftables >/dev/null
cat > /etc/nftables.d-octavio.conf <<'NFT'
table ip octavio { chain pre { type nat hook prerouting priority -100; iifname "usb0" tcp dport 80 redirect to :8080 } }
NFT
grep -q nftables.d-octavio /etc/nftables.conf || echo 'include "/etc/nftables.d-octavio.conf"' >> /etc/nftables.conf
systemctl enable --now nftables avahi-daemon
# 4. the page itself
cp "$(dirname "$0")/octavio-transfert.service" /etc/systemd/system/
systemctl daemon-reload && systemctl enable --now octavio-transfert
echo "Done. Reboot, plug the box into a computer's USB-C port, open http://octavio.local"
