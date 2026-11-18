#!/bin/bash
# Octavio Synth: installs everything on a fresh Raspberry Pi OS (64-bit, Lite or desktop), as the normal user:
#   curl -sL https://raw.githubusercontent.com/jbpaudio-rgb/hardware/main/box/systeme/installer.sh | bash
# Run it again to update (it pulls the latest code and rebuilds). Then: sudo reboot.
# What it does: the packages, the code in ~/hardware, the build, the screen's config.txt lines, and two services
# that start by themselves at power-on: the synth (octavio-box) and the file transfer page (octavio-transfert).
# The USB-C file transfer link is a separate step (it changes how the Pi's USB-C port works):
#   sudo sh ~/hardware/box/systeme/usb-gadget.sh
set -e
U=$(id -un); H=$HOME; SRC=$H/hardware; B=$SRC/build
[ "$U" = root ] && { echo "Lance-le avec ton utilisateur normal (pas sudo) : il demandera le mot de passe au besoin."; exit 1; }

echo "== 1/5 paquets"
sudo apt-get update -qq
sudo apt-get install -y -qq git cmake g++ libasound2-dev python3 avahi-daemon >/dev/null

echo "== 2/5 code (~/hardware)"
if [ -d "$SRC/.git" ]; then git -C "$SRC" pull -q --ff-only; else git clone -q --depth 1 https://github.com/jbpaudio-rgb/hardware "$SRC"; fi

echo "== 3/5 compilation (quelques minutes la premiere fois)"
cmake -S "$SRC/box" -B "$B" -DCMAKE_BUILD_TYPE=Release -DOCTAVIO_SDL=OFF >/dev/null
cmake --build "$B" -j4 2>&1 | grep -E "error|Built target" || true
[ -x "$B/octavio-box" ] || { echo "La compilation a echoue."; exit 1; }

echo "== 4/5 ecran et droits"
CFG=/boot/firmware/config.txt
for L in "dtoverlay=vc4-kms-v3d" "dtoverlay=vc4-kms-dsi-ili9881-5inch"; do
    grep -q "^$L" "$CFG" || echo "$L" | sudo tee -a "$CFG" >/dev/null
done
sudo usermod -aG video,input,audio,render "$U"
mkdir -p "$H/octavio"   # the sounds, multis and samples (what the transfer page shows)

echo "== 5/5 services"
sudo tee /etc/systemd/system/octavio-box.service >/dev/null <<EOF
[Unit]
Description=Octavio Synth
After=sound.target

[Service]
User=$U
WorkingDirectory=$H/octavio
ExecStart=$B/octavio-box --rotate 90
Restart=always
RestartSec=2
Nice=-10
LimitRTPRIO=95
LimitMEMLOCK=infinity

[Install]
WantedBy=multi-user.target
EOF
sed -e "s#^User=.*#User=$U#" -e "s#^ExecStart=.*#ExecStart=/usr/bin/python3 $SRC/box/systeme/transfert.py --root $H/octavio --port 8080#" \
    "$SRC/box/systeme/octavio-transfert.service" | sudo tee /etc/systemd/system/octavio-transfert.service >/dev/null
sudo systemctl daemon-reload
sudo systemctl enable -q octavio-box octavio-transfert
sudo systemctl restart octavio-box octavio-transfert || true

echo
echo "Fini. Redemarre une fois : sudo reboot"
echo "Ensuite le synthe demarre tout seul. Journal : journalctl -u octavio-box -f"
echo "Ecran a l'envers ? sudo sed -i 's/--rotate 90/--rotate 270/' /etc/systemd/system/octavio-box.service && sudo systemctl daemon-reload && sudo systemctl restart octavio-box"
