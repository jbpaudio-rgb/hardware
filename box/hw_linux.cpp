// Linux-only hardware of the Octavio main board (HAT on the Raspberry Pi 5):
//   DIN MIDI on a UART at 31250 baud (termios2 / BOTHER, so no clock tricks are needed)
//   TPA6130A2 headphone amplifier over I2C (i2c-dev)
// Kept in its own file: <asm/termbits.h> cannot live next to <termios.h>.
#include "hw.h"
#ifdef __linux__
#include <asm/termbits.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

int serialMidiOpen(const char* path) {
    const int fd = open(path, O_RDWR | O_NOCTTY);
    if (fd < 0) return -1;
    struct termios2 t {};
    if (ioctl(fd, TCGETS2, &t) < 0) { close(fd); return -1; }
    t.c_cflag = BOTHER | CS8 | CLOCAL | CREAD;
    t.c_iflag = 0; t.c_oflag = 0; t.c_lflag = 0;
    t.c_ispeed = t.c_ospeed = 31250;
    t.c_cc[VMIN] = 1; t.c_cc[VTIME] = 0;
    if (ioctl(fd, TCSETS2, &t) < 0) { close(fd); return -1; }
    return fd;
}
int serialRead(int fd, uint8_t* b, int n) { return (int) read(fd, b, (size_t) n); }
int serialWrite(int fd, const uint8_t* b, int n) { return (int) write(fd, b, (size_t) n); }

static int i2cWrite(int bus, int addr, uint8_t reg, uint8_t val) {
    char dev[32]; snprintf(dev, sizeof dev, "/dev/i2c-%d", bus);
    const int fd = open(dev, O_RDWR); if (fd < 0) return -1;
    int ok = ioctl(fd, I2C_SLAVE_FORCE, addr) >= 0;   // FORCE: the address may be claimed by a disabled driver
    const uint8_t b[2] = { reg, val };
    ok = ok && write(fd, b, 2) == 2;
    close(fd); return ok ? 0 : -1;
}
bool phonesSet(int volume) {   // TPA6130A2 at 0x60: reg 1 = both channels on, stereo; reg 2 = volume 0..63, unmuted
    if (volume < 0) volume = 0; if (volume > 63) volume = 63;
    return i2cWrite(1, 0x60, 1, 0xC0) == 0 && i2cWrite(1, 0x60, 2, (uint8_t) volume) == 0;
}
// the names of the Bluetooth devices BlueZ knows (a BLE MIDI device shows up as an ALSA MIDI port named after it)
std::vector<std::string> bluetoothNames() {
    std::vector<std::string> v; FILE* f = popen("bluetoothctl devices 2>/dev/null", "r"); if (!f) return v;
    char line[256];
    while (fgets(line, sizeof line, f)) {   // "Device AA:BB:CC:DD:EE:FF Name"
        std::string s = line; while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (s.rfind("Device ", 0) != 0 || s.size() < 26) continue;
        const std::string name = s.substr(25); if (!name.empty()) v.push_back(name);
    }
    pclose(f); return v;
}
#else
std::vector<std::string> bluetoothNames() { return {}; }
int serialMidiOpen(const char*) { return -1; }
int serialRead(int, uint8_t*, int) { return -1; }
int serialWrite(int, const uint8_t*, int) { return -1; }
bool phonesSet(int) { return false; }
#endif
