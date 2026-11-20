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
cmake --build "$B" -j2 2>&1 | grep -E "error|Built target" || true   # -j2: fits in a 1 GB Pi (main.cpp takes ~500 MB to compile)
[ -x "$B/octavio-box" ] || { echo "La compilation a echoue."; exit 1; }

echo "== 4/5 ecran et droits"
CFG=/boot/firmware/config.txt
for L in "dtoverlay=vc4-kms-v3d" "dtoverlay=vc4-kms-dsi-ili9881-5inch" "disable_splash=1"; do
    grep -q "^$L" "$CFG" || echo "$L" | sudo tee -a "$CFG" >/dev/null
done
# a quiet start: no Linux text on the screen, no blinking cursor, no login prompt on it (SSH still works); the screen
# stays black until the synth appears
CMD=/boot/firmware/cmdline.txt
for P in quiet loglevel=0 logo.nologo vt.global_cursor_default=0 systemd.show_status=false rd.udev.log_level=0; do
    grep -qw "$P" "$CMD" || sudo sed -i "1s/\$/ $P/" "$CMD"
done
sudo sed -i '1s/console=tty1/console=tty3/' "$CMD"
sudo systemctl disable -q getty@tty1 2>/dev/null || true
sudo usermod -aG video,input,audio,render "$U"
# no sound card of its own yet (the HAT not mounted): the sound goes to the HDMI screen that is plugged in, through
# ALSA's hdmi plugin (the Pi's HDMI only takes IEC958 frames). With the HAT, its card is used and this file is removed.
if ! grep -qi "hifiberry\|pcm5\|pcm1863" /proc/asound/cards 2>/dev/null; then
    P=0; for n in 1 2; do grep -qx connected /sys/class/drm/card*-HDMI-A-$n/status 2>/dev/null && P=$((n - 1)); done
    printf 'pcm.!default { type plug slave.pcm "hdmi:CARD=vc4hdmi%s,DEV=0" }\nctl.!default { type hw card vc4hdmi%s }\n' $P $P | sudo tee /etc/asound.conf >/dev/null
    echo "   son : HDMI $P (pas encore de HAT)"
elif [ -f /etc/asound.conf ] && grep -q vc4hdmi /etc/asound.conf; then sudo rm /etc/asound.conf; fi
mkdir -p "$H/octavio"   # the sounds, multis and samples (what the transfer page shows)

echo "== 5/5 services"
sudo tee /etc/systemd/system/octavio-box.service >/dev/null <<EOF
[Unit]
Description=Octavio Synth
After=sound.target octavio-splash.service
Conflicts=octavio-splash.service

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
# the start-up picture (SINECRAFT, the OCTAVIO logo) as soon as the display driver is up, until the synth takes over
sudo tee /etc/systemd/system/octavio-splash.service >/dev/null <<EOF
[Unit]
Description=Octavio start-up picture
DefaultDependencies=no
After=systemd-udevd.service local-fs.target
Before=octavio-box.service

[Service]
User=$U
ExecStart=$B/octavio-box --splash --rotate 90

[Install]
WantedBy=sysinit.target
EOF
sed -e "s#^User=.*#User=$U#" -e "s#^ExecStart=.*#ExecStart=/usr/bin/python3 $SRC/box/systeme/transfert.py --root $H/octavio --port 8080#" \
    "$SRC/box/systeme/octavio-transfert.service" | sudo tee /etc/systemd/system/octavio-transfert.service >/dev/null
sudo systemctl daemon-reload
sudo systemctl enable -q octavio-box octavio-transfert octavio-splash
sudo systemctl restart octavio-box octavio-transfert || true

echo
echo "Fini. Redemarre une fois : sudo reboot"
echo "Ensuite le synthe demarre tout seul. Journal : journalctl -u octavio-box -f"
echo "Ecran a l'envers ? sudo sed -i 's/--rotate 90/--rotate 270/' /etc/systemd/system/octavio-box.service /etc/systemd/system/octavio-splash.service && sudo systemctl daemon-reload && sudo systemctl restart octavio-box"
