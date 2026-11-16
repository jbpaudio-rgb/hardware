"""Octavio Synth main board: HAT for a Raspberry Pi 5 (audio DAC/ADC, headphone amp, MIDI, power).

Coordinates are HAT millimetres: origin = top-left corner of the Pi 5 board (header edge on top, y down),
so the mounting holes are at (3.5, 3.5), (61.5, 3.5), (3.5, 52.5), (61.5, 52.5) and the 40-pin header
is centred on x = 32.5, y = 3.5. The board extends 21 mm to the left (x < 0) past the Pi's microSD edge:
that strip carries the five 3.5 mm jacks, which form the rear panel. The KiCad board adds OFFSET.

Audio follows the HiFiBerry DAC+ADC Pro topology so the stock Raspberry Pi OS overlay works:
dtoverlay=hifiberry-dacplusadcpro  (PCM5122 @0x4D is clock master, two oscillators enabled by
PCM5122 GPIO3 = 48 kHz family and GPIO6 = 44.1 kHz family, PCM1863 @0x4A, TPA6130A2 @0x60 optional).
MIDI: UART2 (dtoverlay=uart2-pi5): GPIO4 = TX, GPIO5 = RX, TRS type A jacks.
"""

OFFSET = (31.0, 10.0)
LIB = "/home/claude/lib"
ENC_C = []                       # (front-panel only)
PLANES = ("GND", "+3V3A")        # In1.Cu = GND, In2.Cu = +3V3A
POWER_NETS = ("+5V", "+5V_LED")
MID_NETS = ("+3V3P",)
TEXT_POS = (40, 52)

FP = {
    "R0402": f"{LIB}/fp/Resistor_SMD.pretty/R_0402_1005Metric.kicad_mod",
    "C0402": f"{LIB}/fp/Capacitor_SMD.pretty/C_0402_1005Metric.kicad_mod",
    "C0603": f"{LIB}/fp/Capacitor_SMD.pretty/C_0603_1608Metric.kicad_mod",
    "SOIC8": f"{LIB}/fp/Package_SO.pretty/SOIC-8_3.9x4.9mm_P1.27mm.kicad_mod",
    "TSSOP28": f"{LIB}/fp/Package_SO.pretty/TSSOP-28_4.4x9.7mm_P0.65mm.kicad_mod",
    "TSSOP30": f"{LIB}/fp/Package_SO.pretty/TSSOP-30_4.4x7.8mm_P0.5mm.kicad_mod",
    "QFN20": f"{LIB}/fp/Package_DFN_QFN.pretty/QFN-20-1EP_4x4mm_P0.5mm_EP2.7x2.7mm_ThermalVias.kicad_mod",
    "SMDIP6": f"{LIB}/fp/Package_DIP.pretty/SMDIP-6_W9.53mm.kicad_mod",
    "SOT23": f"{LIB}/fp/Package_TO_SOT_SMD.pretty/SOT-23.kicad_mod",
    "SOT353": f"{LIB}/fp/Package_TO_SOT_SMD.pretty/SOT-353_SC-70-5.kicad_mod",
    "OSC": f"{LIB}/fp/Oscillator.pretty/Oscillator_SMD_Abracon_ASE-4Pin_3.2x2.5mm.kicad_mod",
    "SOD123": f"{LIB}/fp/Diode_SMD.pretty/D_SOD-123.kicad_mod",
    "F1206": f"{LIB}/fp/Fuse.pretty/Fuse_1206_3216Metric.kicad_mod",
    "JSTPH2": f"{LIB}/fp/Connector_JST.pretty/JST_PH_S2B-PH-SM4-TB_1x02-1MP_P2.00mm_Horizontal.kicad_mod",
    "JSTPH3V": f"{LIB}/fp/Connector_JST.pretty/JST_PH_B3B-PH-K_1x03_P2.00mm_Vertical.kicad_mod",
    "HDR2": f"{LIB}/fp/Connector_PinHeader_2.54mm.pretty/PinHeader_1x02_P2.54mm_Vertical.kicad_mod",
    "GPIO40": f"{LIB}/fp/Connector_PinHeader_2.54mm.pretty/PinHeader_2x20_P2.54mm_Vertical.kicad_mod",
    "JACK": f"{LIB}/fp/Connector_Audio.pretty/Jack_3.5mm_CUI_SJ1-3533NG_Horizontal.kicad_mod",
    "HOLE": f"{LIB}/fp/MountingHole.pretty/MountingHole_2.7mm_M2.5.kicad_mod",
}
FPNAME = {
    "R0402": "Resistor_SMD:R_0402_1005Metric", "C0402": "Capacitor_SMD:C_0402_1005Metric",
    "C0603": "Capacitor_SMD:C_0603_1608Metric", "SOIC8": "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
    "TSSOP28": "Package_SO:TSSOP-28_4.4x9.7mm_P0.65mm", "TSSOP30": "Package_SO:TSSOP-30_4.4x7.8mm_P0.5mm",
    "QFN20": "Package_DFN_QFN:QFN-20-1EP_4x4mm_P0.5mm_EP2.7x2.7mm_ThermalVias",
    "SMDIP6": "Package_DIP:SMDIP-6_W9.53mm", "SOT23": "Package_TO_SOT_SMD:SOT-23",
    "SOT353": "Package_TO_SOT_SMD:SOT-353_SC-70-5",
    "OSC": "Oscillator:Oscillator_SMD_Abracon_ASE-4Pin_3.2x2.5mm", "SOD123": "Diode_SMD:D_SOD-123",
    "F1206": "Fuse:Fuse_1206_3216Metric",
    "JSTPH2": "Connector_JST:JST_PH_S2B-PH-SM4-TB_1x02-1MP_P2.00mm_Horizontal",
    "JSTPH3V": "Connector_JST:JST_PH_B3B-PH-K_1x03_P2.00mm_Vertical",
    "HDR2": "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical",
    "GPIO40": "Connector_PinHeader_2.54mm:PinHeader_2x20_P2.54mm_Vertical",
    "JACK": "Connector_Audio:Jack_3.5mm_CUI_SJ1-3533NG_Horizontal",
    "HOLE": "MountingHole:MountingHole_2.7mm_M2.5",
}

LCSC = {
    "10k": "C25744", "1k": "C11702", "470R": "C25117", "220R": "C25091", "22R": "C25092",
    "4.7k": "C25900", "2.2k": "C25879",
    "100nF": "C1525", "1uF": "C52923", "2.2uF": "C23630", "10uF": "C19702", "2.2nF": "C1531",
    "PCM5122PWR": "C962071", "PCM1863DBTR": "C529569", "TPA6130A2RTJR": "C130046",
    "24.576MHz": "C437128", "22.5792MHz": "C2901572", "H11L1SR2M": "C20082",
    "CAT24C32WI-GT3": "C81193", "74AHCT1G125": "C12495", "XC6206P332MR": "C5446",
    "1N4148W": "C81598", "SMD1206P200TF": "C20988", "S2B-PH-SM4-TB": "C295747", "S3B-PH-SM4-TB": "C265101",
}

parts = []


def add(ref, fp, value, x, y, rot=0, pads=None, sym=None, dnp=False, lcsc=None):
    parts.append(dict(ref=ref, fp=fp, value=value, x=x, y=y, rot=rot, pads=pads or {}, sym=sym,
                      dnp=dnp, lcsc=lcsc if lcsc is not None else LCSC.get(value, "")))


_n = {"R": 0, "C": 0}


def R(value, a, b, x, y, rot=0, fp="R0402"):
    _n["R"] += 1
    add(f"R{_n['R']}", fp, value, x, y, rot, {"1": a, "2": b}, sym=("R", ["1", "2"]))


def C(value, a, b, x, y, rot=0, fp="C0402"):
    _n["C"] += 1
    add(f"C{_n['C']}", fp, value, x, y, rot, {"1": a, "2": b}, sym=("C", ["1", "2"]))


# ---------------- Raspberry Pi 40-pin header (female socket under the HAT) ----------------
GPIO = {1: "+3V3P", 2: "+5V", 3: "I2C_SDA", 4: "+5V", 5: "I2C_SCL", 6: "GND", 7: "MIDI_TX", 9: "GND",
        12: "I2S_BCK", 14: "GND", 17: "+3V3P", 20: "GND", 25: "GND", 27: "ID_SD", 28: "ID_SC",
        29: "MIDI_RX", 30: "GND", 34: "GND", 35: "I2S_LRCK", 38: "I2S_ADC", 39: "GND", 40: "I2S_DAC"}
PI_NAMES = {1: "3V3", 2: "5V", 3: "GPIO2/SDA", 4: "5V", 5: "GPIO3/SCL", 6: "GND", 7: "GPIO4/TX2",
            8: "GPIO14", 9: "GND", 10: "GPIO15", 11: "GPIO17", 12: "GPIO18/BCK", 13: "GPIO27", 14: "GND",
            15: "GPIO22", 16: "GPIO23", 17: "3V3", 18: "GPIO24", 19: "GPIO10", 20: "GND", 21: "GPIO9",
            22: "GPIO25", 23: "GPIO11", 24: "GPIO8", 25: "GND", 26: "GPIO7", 27: "ID_SD", 28: "ID_SC",
            29: "GPIO5/RX2", 30: "GND", 31: "GPIO6", 32: "GPIO12", 33: "GPIO13", 34: "GND",
            35: "GPIO19/LRCK", 36: "GPIO16", 37: "GPIO26", 38: "GPIO20/DIN", 39: "GND", 40: "GPIO21/DOUT"}
# PinHeader footprint rotated 90: pin 1 lands on the Pi's pin 1 (inner row), pin 2 on the board edge row
add("J1", "GPIO40", "2x20 socket 2.54", 32.5 - 24.13, 4.77, 90, {str(k): v for k, v in GPIO.items()},
    sym=("Raspberry Pi GPIO", [str(k) for k in range(1, 41)]), lcsc="C2977589")
parts[-1]["side"] = "Bottom"   # the socket sits under the HAT, on the Pi's header
for i, (hx, hy) in enumerate([(3.5, 3.5), (61.5, 3.5), (3.5, 52.5), (61.5, 52.5)]):
    add(f"H{i + 1}", "HOLE", "M2.5", hx, hy, 0, {}, sym=None, dnp=True, lcsc="")
C("10uF", "+5V", "GND", 14.5, 8.2, 0, fp="C0603")
C("100nF", "+5V", "GND", 17.5, 8.2, 0)

# ---------------- rear jacks (3.5 mm TRS, facing left) ----------------
JX = -19.8                    # jack body front flush with the board edge (rear wall of the case)
JACKS = [("J5", "MIDI IN", 5.0, {"T": "MIDI_IN5", "R": "MIDI_IN4"}),
         ("J6", "MIDI OUT", 18.0, {"T": "MIDI_OUT5", "R": "MIDI_OUT4", "S": "GND"}),
         ("J4", "LINE IN", 31.0, {"T": "LIN_L", "R": "LIN_R", "S": "GND"}),
         ("J3", "PHONES", 48.3, {"T": "HP_L", "R": "HP_R", "S": "GND"})]
for ref, label, jy, pads in JACKS:
    add(ref, "JACK", "SJ1-3533NG", JX, jy, 90, pads, sym=(f"Jack {label}", ["T", "R", "S"]), lcsc="C4992459")

# LINE OUT: two 1/4" jacks screwed to the rear panel, wired to J2 (1 = left, 2 = right, 3 = ground)
add("J2", "JSTPH3V", "B3B-PH-K-S", -14.0, 39.8, 0, {"1": "OUT_L", "2": "OUT_R", "3": "GND"}, lcsc="C131339",
    sym=("Sortie ligne (jacks 1/4 du panneau)", ["1", "2", "3"]))

# ---------------- MIDI ----------------
# IN: pin 4 (ring) -> 220R -> opto LED -> pin 5 (tip); 1N4148 reverse protection; shield not connected
R("220R", "MIDI_IN4", "MIDI_A", -1.5, 8.0, 90)
add("D1", "SOD123", "1N4148W", -1.5, 12.5, 90, {"1": "MIDI_A", "2": "MIDI_IN5"}, sym=("D", ["1", "2"]))
add("U6", "SMDIP6", "H11L1SR2M", 6.5, 13.0, 0,
    {"1": "MIDI_A", "2": "MIDI_IN5", "4": "MIDI_RX", "5": "GND", "6": "+3V3P"},
    sym=("H11L1", ["1", "2", "3", "4", "5", "6"]))
C("100nF", "+3V3P", "GND", 13.8, 10.0, 90)
R("1k", "MIDI_RX", "+3V3P", 13.8, 15.5, 90)
# OUT (5 V MIDI): pin 4 <- 220R <- +5V, pin 5 <- 220R <- buffered TX
add("U7", "SOT353", "74AHCT1G125", 17.0, 13.0, 0,
    {"1": "GND", "2": "MIDI_TX", "3": "GND", "4": "MIDI_TXB", "5": "+5V"},
    sym=("74AHCT1G125", ["1", "2", "3", "4", "5"]))
C("100nF", "+5V", "GND", 19.6, 13.0, 90)
R("220R", "MIDI_TXB", "MIDI_OUT5", -1.5, 17.6, 90)
R("220R", "+5V", "MIDI_OUT4", -1.5, 19.6, 90)

# ---------------- clocks: 24.576 MHz (48k family) and 22.5792 MHz (44.1k family) ----------------
# pin 1 OE, 2 GND, 3 OUT, 4 VDD; outputs tri-stated when disabled, joined through 22R
add("Y1", "OSC", "24.576MHz", 25.5, 36.0, 0, {"1": "OSC48_EN", "2": "GND", "3": "OSC48_OUT", "4": "+3V3A"},
    sym=("Oscillateur 24.576 MHz", ["1", "2", "3", "4"]))
add("Y2", "OSC", "22.5792MHz", 25.5, 41.5, 0, {"1": "OSC44_EN", "2": "GND", "3": "OSC44_OUT", "4": "+3V3A"},
    sym=("Oscillateur 22.5792 MHz", ["1", "2", "3", "4"]))
C("100nF", "+3V3A", "GND", 29.0, 35.6, 90)
C("100nF", "+3V3A", "GND", 29.0, 41.1, 90)
R("22R", "OSC48_OUT", "MCLK", 22.4, 37.6, 90)
R("22R", "OSC44_OUT", "MCLK", 22.4, 40.0, 90)

# ---------------- PCM1863 ADC (line in), I2C 0x4A ----------------
ADC_NAMES = {"1": "VINL2", "2": "VINR2", "3": "VINL1", "4": "VINR1", "5": "MICBIAS", "6": "VREF", "7": "AGND",
             "8": "AVDD", "9": "XO", "10": "XI", "11": "LDO", "12": "DGND", "13": "DVDD", "14": "IOVDD",
             "15": "SCKI", "16": "LRCK", "17": "BCK", "18": "DOUT", "19": "GPIO3", "20": "GPIO2",
             "21": "GPIO1", "22": "GPIO0", "23": "SDA", "24": "SCL", "25": "MS/AD", "26": "MD0",
             "27": "VINL4", "28": "VINR4", "29": "VINL3", "30": "VINR3"}
add("U2", "TSSOP30", "PCM1863DBTR", 14.0, 24.0, 0,
    {"3": "LIN_LC", "4": "LIN_RC", "6": "ADC_VREF", "7": "GND", "8": "+3V3A", "10": "GND", "11": "ADC_LDO",
     "12": "GND", "13": "+3V3A", "14": "+3V3A", "15": "MCLK", "16": "I2S_LRCK", "17": "I2S_BCK",
     "18": "I2S_ADC", "23": "I2C_SDA", "24": "I2C_SCL", "25": "GND", "26": "GND"},
    sym=("PCM1863", [str(k) for k in range(1, 31)]))
C("2.2uF", "LIN_L", "LIN_LC", 3.5, 20.8, 0, fp="C0603")
C("2.2uF", "LIN_R", "LIN_RC", 3.5, 22.6, 0, fp="C0603")
C("1uF", "ADC_VREF", "GND", 8.5, 23.0, 0)
C("100nF", "+3V3A", "GND", 8.5, 24.2, 0)                      # AVDD
C("100nF", "ADC_LDO", "GND", 8.5, 25.4, 0)
C("100nF", "+3V3A", "GND", 8.5, 26.6, 0)                      # DVDD
C("100nF", "+3V3A", "GND", 8.5, 27.8, 0)                      # IOVDD
C("10uF", "+3V3A", "GND", 5.6, 24.6, 0, fp="C0603")
C("10uF", "ADC_LDO", "GND", 5.6, 26.4, 0, fp="C0603")

# ---------------- PCM5122 DAC (line out), I2C 0x4D, clock master ----------------
DAC_NAMES = {"1": "CPVDD", "2": "CAPP", "3": "CPGND", "4": "CAPM", "5": "VNEG", "6": "OUTL", "7": "OUTR",
             "8": "AVDD", "9": "AGND", "10": "VCOM", "11": "SDA", "12": "SCL", "13": "GPIO5", "14": "GPIO4",
             "15": "GPIO3", "16": "ADR2", "17": "MODE1", "18": "MODE2", "19": "GPIO6", "20": "SCK",
             "21": "BCK", "22": "DIN", "23": "LRCK", "24": "ADR1", "25": "XSMT", "26": "LDOO", "27": "DGND",
             "28": "DVDD"}
add("U1", "TSSOP28", "PCM5122PWR", 14.0, 37.4, 0,
    {"1": "+3V3A", "2": "DAC_CAPP", "3": "GND", "4": "DAC_CAPM", "5": "DAC_VNEG", "6": "DAC_OUTL",
     "7": "DAC_OUTR", "8": "+3V3A", "9": "GND", "10": "GND", "11": "I2C_SDA", "12": "I2C_SCL",
     "15": "OSC48_EN", "16": "GND", "17": "GND", "18": "+3V3A", "19": "OSC44_EN", "20": "MCLK",
     "21": "I2S_BCK", "22": "I2S_DAC", "23": "I2S_LRCK", "24": "+3V3A", "25": "DAC_XSMT", "26": "DAC_LDOO",
     "27": "GND", "28": "+3V3A"},
    sym=("PCM5122", [str(k) for k in range(1, 29)]))
C("100nF", "+3V3A", "GND", 9.0, 32.0, 90)                     # CPVDD
C("2.2uF", "DAC_CAPP", "DAC_CAPM", 9.0, 34.6, 90, fp="C0603")  # flying cap
C("2.2uF", "DAC_VNEG", "GND", 9.0, 37.7, 90, fp="C0603")
C("100nF", "+3V3A", "GND", 9.0, 40.3, 90)                     # AVDD
C("10uF", "+3V3A", "GND", 6.6, 40.6, 0, fp="C0603")
R("470R", "DAC_OUTL", "OUT_L", 6.6, 36.3, 0)                  # output RC filter
R("470R", "DAC_OUTR", "OUT_R", 6.6, 37.4, 0)
C("2.2nF", "OUT_L", "GND", 4.6, 36.3, 0)
C("2.2nF", "OUT_R", "GND", 4.6, 37.4, 0)
C("100nF", "+3V3A", "GND", 19.0, 31.8, 90)                    # DVDD
C("10uF", "+3V3A", "GND", 20.9, 32.2, 90, fp="C0603")
C("1uF", "DAC_LDOO", "GND", 19.0, 34.0, 90)
R("10k", "DAC_XSMT", "+3V3A", 19.0, 36.0, 90)
R("10k", "OSC44_EN", "GND", 19.0, 39.6, 90)
R("10k", "OSC48_EN", "GND", 19.0, 42.0, 90)

# ---------------- TPA6130A2 headphone amp (5 V), I2C 0x60 ----------------
HP_NAMES = {"1": "LEFTINM", "2": "LEFTINP", "3": "GND", "4": "RIGHTINP", "5": "RIGHTINM", "6": "SD",
            "7": "SDA", "8": "SCL", "9": "GND", "10": "GND", "11": "HPRIGHT", "12": "VDD", "13": "GND",
            "14": "HPLEFT", "15": "CPVSS", "16": "CPVSS", "17": "CPN", "18": "CPP", "19": "GND", "20": "VDD",
            "21": "EP"}
add("U3", "QFN20", "TPA6130A2RTJR", 11.0, 46.5, 0,
    {"1": "TPA_LM", "2": "TPA_LP", "3": "GND", "4": "TPA_RP", "5": "TPA_RM", "6": "TPA_SD", "7": "I2C_SDA",
     "8": "I2C_SCL", "9": "GND", "10": "GND", "11": "HP_R", "12": "+5V", "13": "GND", "14": "HP_L",
     "15": "TPA_CPVSS", "16": "TPA_CPVSS", "17": "TPA_CPN", "18": "TPA_CPP", "19": "GND", "20": "+5V",
     "21": "GND"},
    sym=("TPA6130A2", [str(k) for k in range(1, 22)]))
C("1uF", "TPA_CPP", "TPA_CPN", 11.3, 43.3, 0)                 # flying cap
C("1uF", "+5V", "GND", 7.6, 43.4, 90)                         # VDD (charge pump)
C("1uF", "TPA_CPVSS", "GND", 14.3, 44.4, 90)
C("1uF", "+5V", "GND", 14.3, 47.8, 270)                       # VDD (analog)
C("10uF", "+5V", "GND", 16.6, 46.4, 90, fp="C0603")
C("1uF", "HPDIV_L", "TPA_LM", 7.0, 45.3, 0)                   # inputs: single-ended into INM, INP to ground
C("1uF", "GND", "TPA_LP", 7.0, 46.3, 0)
C("1uF", "GND", "TPA_RP", 7.0, 47.3, 0)
C("1uF", "HPDIV_R", "TPA_RM", 7.0, 48.3, 0)
R("4.7k", "OUT_L", "HPDIV_L", 4.0, 40.0, 0)                   # 2.1 Vrms line level -> about 0.67 Vrms
R("2.2k", "HPDIV_L", "GND", 4.0, 41.0, 0)
R("4.7k", "OUT_R", "HPDIV_R", 4.0, 42.0, 0)
R("2.2k", "HPDIV_R", "GND", 4.0, 43.0, 0)
R("10k", "TPA_SD", "+3V3A", 11.0, 50.2, 0)

# ---------------- 3.3 V audio supply ----------------
add("U8", "SOT23", "XC6206P332MR", 30.0, 47.0, 0, {"1": "GND", "2": "+3V3A", "3": "+5V"},
    sym=("XC6206 3.3V", ["1", "2", "3"]))
C("1uF", "+5V", "GND", 27.4, 47.0, 90)
C("1uF", "+3V3A", "GND", 32.6, 47.0, 90)
C("10uF", "+3V3A", "GND", 34.6, 47.0, 90, fp="C0603")

# ---------------- HAT ID EEPROM (Pi's ID_SD/ID_SC), address 0x50 ----------------
add("U5", "SOIC8", "CAT24C32WI-GT3", 40.0, 13.0, 0,
    {"1": "GND", "2": "GND", "3": "GND", "4": "GND", "5": "ID_SD", "6": "ID_SC", "7": "EE_WP", "8": "+3V3P"},
    sym=("CAT24C32", ["1", "2", "3", "4", "5", "6", "7", "8"]))
C("100nF", "+3V3P", "GND", 44.6, 10.0, 90)
R("10k", "EE_WP", "+3V3P", 45.6, 13.4, 90)
add("JP1", "R0402", "0R (ecrire EEPROM)", 47.2, 13.4, 90, {"1": "EE_WP", "2": "GND"}, sym=("JP", ["1", "2"]),
    dnp=True, lcsc="")
R("4.7k", "ID_SD", "+3V3P", 45.6, 16.6, 90)
R("4.7k", "ID_SC", "+3V3P", 47.2, 16.6, 90)

# ---------------- 5 V outputs: front-panel LEDs (fused) and the touch display ----------------
add("F1", "F1206", "SMD1206P200TF", 50.0, 30.0, 90, {"1": "+5V", "2": "+5V_LED"}, sym=("Fusible 2A", ["1", "2"]))
add("J7", "JSTPH2", "S2B-PH-SM4-TB", 58.0, 25.0, 0, {"1": "+5V_LED", "2": "GND", "MP": "GND"},
    sym=("5V LED facade", ["1", "2"]))
C("10uF", "+5V_LED", "GND", 53.0, 33.0, 90, fp="C0603")
add("J8", "HDR2", "1x2 male 2.54", 54.0, 42.0, 90, {"1": "+5V", "2": "GND"}, sym=("5V ecran", ["1", "2"]),
    lcsc="C2905434")
C("10uF", "+5V", "GND", 47.0, 40.0, 90, fp="C0603")

OUTLINE = [(-21, 0), (65, 0), (65, 56.5), (-21, 56.5)]
OUTLINE_IN = [(-20.5, 0.5), (64.5, 0.5), (64.5, 56.0), (-20.5, 56.0)]

PIN_NAMES = {"J1": {str(k): v for k, v in PI_NAMES.items()}, "U1": DAC_NAMES, "U2": ADC_NAMES,
             "U3": HP_NAMES,
             "U5": {"1": "A0", "2": "A1", "3": "A2", "4": "VSS", "5": "SDA", "6": "SCL", "7": "WP", "8": "VCC"},
             "U6": {"1": "A", "2": "K", "3": "NC", "4": "VO", "5": "GND", "6": "VCC"},
             "U7": {"1": "/OE", "2": "A", "3": "GND", "4": "Y", "5": "VCC"},
             "U8": {"1": "GND", "2": "VOUT", "3": "VIN"},
             "Y1": {"1": "OE", "2": "GND", "3": "OUT", "4": "VDD"},
             "Y2": {"1": "OE", "2": "GND", "3": "OUT", "4": "VDD"}}

if __name__ == "__main__":
    nets = sorted({n for p in parts for n in p["pads"].values()})
    print(len(parts), "parts,", len(nets), "nets")
