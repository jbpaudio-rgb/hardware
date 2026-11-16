// Main-board hardware (Linux only; the functions fail harmlessly elsewhere). See hw_linux.cpp.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

int serialMidiOpen(const char* path);          // DIN MIDI UART at 31250 baud; -1 if it cannot be opened
int serialRead(int fd, uint8_t* b, int n);     // blocking
int serialWrite(int fd, const uint8_t* b, int n);
bool phonesSet(int volume);                    // headphone amp volume 0..63 (and switch it on)
std::vector<std::string> bluetoothNames();   // the Bluetooth devices BlueZ knows (bluetoothctl), to tell BLE MIDI ports apart
