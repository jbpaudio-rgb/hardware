"""Octavio Synth front-panel board: parts, positions and connections.

Coordinates are front-panel millimetres (origin = top-left corner of the 300 x 155 mm panel, y down),
the same as the enclosure doc. The KiCad board adds OFFSET to them.
Each part: ref, footprint (lib:name), value, LCSC number, x, y, rotation (deg, KiCad CCW), pad -> net,
plus 'sym' (pin names for the schematic) and 'dnp' (not assembled by JLCPCB).
"""
import math

OFFSET = (20.0, 20.0)
LIB = "/home/claude/lib"
FP = {
    "R0402": f"{LIB}/fp/Resistor_SMD.pretty/R_0402_1005Metric.kicad_mod",
    "C0402": f"{LIB}/fp/Capacitor_SMD.pretty/C_0402_1005Metric.kicad_mod",
    "C0603": f"{LIB}/fp/Capacitor_SMD.pretty/C_0603_1608Metric.kicad_mod",
    "R0603": f"{LIB}/fp/Resistor_SMD.pretty/R_0603_1608Metric.kicad_mod",
    "RP2040": f"{LIB}/rp/PCB/Libraries/Footprints/RP2040-QFN-56.kicad_mod",
    "SOIC8W": f"{LIB}/fp/Package_SO.pretty/SOIC-8_5.23x5.23mm_P1.27mm.kicad_mod",
    "SOIC16": f"{LIB}/fp/Package_SO.pretty/SOIC-16_3.9x9.9mm_P1.27mm.kicad_mod",
    "SOT23": f"{LIB}/fp/Package_TO_SOT_SMD.pretty/SOT-23.kicad_mod",
    "SOT353": f"{LIB}/fp/Package_TO_SOT_SMD.pretty/SOT-353_SC-70-5.kicad_mod",
    "XTAL": f"{LIB}/fp/Crystal.pretty/Crystal_SMD_3225-4Pin_3.2x2.5mm.kicad_mod",
    "SOD123": f"{LIB}/fp/Diode_SMD.pretty/D_SOD-123.kicad_mod",
    "USBC": f"{LIB}/fp/Connector_USB.pretty/USB_C_Receptacle_HRO_TYPE-C-31-M-12.kicad_mod",
    "JSTSH4": f"{LIB}/fp/Connector_JST.pretty/JST_SH_SM04B-SRSS-TB_1x04-1MP_P1.00mm_Horizontal.kicad_mod",
    "JSTPH2": f"{LIB}/fp/Connector_JST.pretty/JST_PH_S2B-PH-SM4-TB_1x02-1MP_P2.00mm_Horizontal.kicad_mod",
    "HDR3": f"{LIB}/fp/Connector_PinHeader_2.54mm.pretty/PinHeader_1x03_P2.54mm_Vertical.kicad_mod",
    "TACT": f"{LIB}/fp/Button_Switch_SMD.pretty/SW_SPST_SKQG_WithoutStem.kicad_mod",
    "ENC": f"{LIB}/fp/Rotary_Encoder.pretty/RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm.kicad_mod",
    "POT": f"{LIB}/fp/Potentiometer_THT.pretty/Potentiometer_Alps_RK09L_Single_Vertical.kicad_mod",
    "CHOC": f"{LIB}/ks/library/footprints/Switch_Keyboard_Kailh.pretty/SW_Kailh_Choc_V1_1.00u.kicad_mod",
    "LED": "/home/claude/fpb/footprints/LED_WS2812B-2020.kicad_mod",
    "HOLE": f"{LIB}/fp/MountingHole.pretty/MountingHole_3.2mm_M3.kicad_mod",
}
FPNAME = {  # library:name written in the board / schematic
    "R0402": "Resistor_SMD:R_0402_1005Metric", "C0402": "Capacitor_SMD:C_0402_1005Metric",
    "C0603": "Capacitor_SMD:C_0603_1608Metric", "R0603": "Resistor_SMD:R_0603_1608Metric",
    "RP2040": "Octavio:RP2040-QFN-56", "SOIC8W": "Package_SO:SOIC-8_5.23x5.23mm_P1.27mm",
    "SOIC16": "Package_SO:SOIC-16_3.9x9.9mm_P1.27mm", "SOT23": "Package_TO_SOT_SMD:SOT-23",
    "SOT353": "Package_TO_SOT_SMD:SOT-353_SC-70-5", "XTAL": "Crystal:Crystal_SMD_3225-4Pin_3.2x2.5mm",
    "SOD123": "Diode_SMD:D_SOD-123", "USBC": "Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12",
    "JSTSH4": "Connector_JST:JST_SH_SM04B-SRSS-TB_1x04-1MP_P1.00mm_Horizontal",
    "JSTPH2": "Connector_JST:JST_PH_S2B-PH-SM4-TB_1x02-1MP_P2.00mm_Horizontal",
    "HDR3": "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
    "TACT": "Button_Switch_SMD:SW_SPST_SKQG_WithoutStem",
    "ENC": "Rotary_Encoder:RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm",
    "POT": "Potentiometer_THT:Potentiometer_Alps_RK09L_Single_Vertical",
    "CHOC": "Switch_Keyboard_Kailh:SW_Kailh_Choc_V1_1.00u", "LED": "Octavio:LED_WS2812B-2020",
    "HOLE": "MountingHole:MountingHole_3.2mm_M3",
}

# LCSC numbers. "?" = checked part family, number to confirm in the JLCPCB BOM tool.
LCSC = {
    "10k": "C25744", "5.1k": "C25905", "1uF": "C52923", "27R": "C25100", "27pF": "C1557",
    "100nF": "C1525", "10nF": "C15195", "1k": "C11702", "330R": "C25104", "10uF": "C19702",
    "RP2040": "C2040", "W25Q16JVSSIQ": "C131025", "74HC165D": "C5613", "74AHCT1G125": "C12495",
    "XC6206P332MR": "C5446", "X322512MSB4SI": "C9002", "B5819W": "C8598", "WS2812B-2020": "C965555",
    "TYPE-C-31-M-12": "C165948", "SM04B-SRSS-TB": "C160404", "S2B-PH-SM4-TB": "C295747",
    "TS-1187A-B-A-B": "C318884", "2N7002": "C8545", "100k": "C25741",
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


XD3, YD3, XQ1, YQ1, XJ5, YJ5, RJ5 = 285.0, 143.5, 262.5, 141.0, 270.5, 144.0, 90
# ---------------- connectors and power (right edge) ----------------
usb = {p: "GND" for p in ("A1", "B12", "A12", "B1", "S1")}
usb.update({p: "VBUS_C" for p in ("A4", "B9", "A9", "B4")})
usb.update({"A5": "CC1", "B5": "CC2", "A6": "USB_DP", "B6": "USB_DP", "A7": "USB_DN", "B7": "USB_DN"})
add("J1", "USBC", "TYPE-C-31-M-12", 292.3, 112, 90, usb,
    sym=("USB-C (test)", ["A1", "A4", "A5", "B5", "A6", "B6", "A7", "B7", "S1"]))
R("5.1k", "CC1", "GND", 285.5, 119, 90)
R("5.1k", "CC2", "GND", 287.5, 119, 90)
add("D1", "SOD123", "B5819W", 285.5, 111, 90, {"1": "+5V", "2": "VBUS_C"}, sym=("D", ["1", "2"]))
add("J2", "JSTSH4", "SM04B-SRSS-TB", 291.5, 125, 90,
    {"1": "GND", "2": "VBUS_J", "3": "USB_DP", "4": "USB_DN", "MP": "GND"},
    sym=("USB interne", ["1", "2", "3", "4"]))
add("D2", "SOD123", "B5819W", 285.5, 125, 90, {"1": "+5V", "2": "VBUS_J"}, sym=("D", ["1", "2"]))
add("J3", "JSTPH2", "S2B-PH-SM4-TB", 290.5, 137.5, 90, {"1": "+5V_LED", "2": "GND", "MP": "GND"},
    sym=("5V LED", ["1", "2"]))
add("JP1", "R0603", "0R (test USB)", 284.5, 134, 90, {"1": "+5V", "2": "+5V_LED"}, sym=("JP", ["1", "2"]),
    dnp=True, lcsc="")
C("10uF", "+5V_LED", "GND", 282.5, 139.5, 90, fp="C0603")
# the machine on / off (v1.1): the RP2040 also runs from the 5 V header (through D3) while the Pi is halted, and Q1
# pulls the Pi 5's power-button pads (its J2, a 2-pin header soldered there, cable to J5) low to switch it on
add("D3", "SOD123", "B5819W", XD3, YD3, 0, {"1": "+5V", "2": "+5V_LED"}, sym=("D", ["1", "2"]))
add("Q1", "SOT23", "2N7002", XQ1, YQ1, 0, {"1": "PWR_KEY", "2": "GND", "3": "PI_PWR_BUT"}, sym=("2N7002", ["1", "2", "3"]))
add("R32", "R0402", "100k", XQ1 - 2.5, YQ1, 90, {"1": "PWR_KEY", "2": "GND"}, sym=("R", ["1", "2"]))   # after the old R1-R31: their numbers stay
add("J5", "JSTPH2", "S2B-PH-SM4-TB", XJ5, YJ5, RJ5, {"1": "PI_PWR_BUT", "2": "GND", "MP": "GND"},
    sym=("Pi 5 J2 (bouton)", ["1", "2"]))
C("10uF", "+5V_LED", "GND", 280.5, 139.5, 90, fp="C0603")

# ---------------- RP2040 cluster (bottom strip, x 203-222) ----------------
gp = {}
encp = [2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 27, 28]   # GPIO0..17
for i in range(9):
    gp[str(encp[2 * i])] = f"ENC{i + 1}_A"
    gp[str(encp[2 * i + 1])] = f"ENC{i + 1}_B"
rp = {**gp}
for p in (1, 10, 22, 33, 42, 49, 43, 44, 48):
    rp[str(p)] = "+3V3"
for p in (23, 50, 45):
    rp[str(p)] = "+1V1"
rp.update({"57": "GND", "19": "GND", "20": "XIN", "21": "XOUT_R", "24": "SWCLK", "25": "SWDIO", "26": "RUN",
           "46": "USB_DN_R", "47": "USB_DP_R", "51": "QSPI_SD3", "52": "QSPI_SCLK", "53": "QSPI_SD0",
           "54": "QSPI_SD2", "55": "QSPI_SD1", "56": "QSPI_SS", "29": "SR_CLK", "30": "SR_LD", "31": "SR_QH",
           "34": "LED_D3V3", "38": "VOL", "40": "PWR_KEY"})
RP_NAMES = {'1': 'IOVDD', '2': 'GPIO0', '3': 'GPIO1', '4': 'GPIO2', '5': 'GPIO3', '6': 'GPIO4', '7': 'GPIO5',
            '8': 'GPIO6', '9': 'GPIO7', '10': 'IOVDD', '11': 'GPIO8', '12': 'GPIO9', '13': 'GPIO10', '14': 'GPIO11',
            '15': 'GPIO12', '16': 'GPIO13', '17': 'GPIO14', '18': 'GPIO15', '19': 'TESTEN', '20': 'XIN',
            '21': 'XOUT', '22': 'IOVDD', '23': 'DVDD', '24': 'SWCLK', '25': 'SWDIO', '26': 'RUN', '27': 'GPIO16',
            '28': 'GPIO17', '29': 'GPIO18', '30': 'GPIO19', '31': 'GPIO20', '32': 'GPIO21', '33': 'IOVDD',
            '34': 'GPIO22', '35': 'GPIO23', '36': 'GPIO24', '37': 'GPIO25', '38': 'GPIO26/ADC0',
            '39': 'GPIO27/ADC1', '40': 'GPIO28/ADC2', '41': 'GPIO29/ADC3', '42': 'IOVDD', '43': 'ADC_AVDD',
            '44': 'VREG_VIN', '45': 'VREG_VOUT', '46': 'USB_DM', '47': 'USB_DP', '48': 'USB_VDD', '49': 'IOVDD',
            '50': 'DVDD', '51': 'QSPI_SD3', '52': 'QSPI_SCLK', '53': 'QSPI_SD0', '54': 'QSPI_SD2',
            '55': 'QSPI_SD1', '56': 'QSPI_SS', '57': 'GND'}
add("U2", "RP2040", "RP2040", 212.5, 121.5, 0, rp, sym=("RP2040", [str(i) for i in range(1, 58)]))
add("U3", "SOIC8W", "W25Q16JVSSIQ", 212.5, 112, 0,
    {"1": "QSPI_SS", "2": "QSPI_SD1", "3": "QSPI_SD2", "4": "GND", "5": "QSPI_SD0", "6": "QSPI_SCLK",
     "7": "QSPI_SD3", "8": "+3V3"},
    sym=("W25Q16JV", ["1", "2", "3", "4", "5", "6", "7", "8"]))
FLASH_NAMES = {"1": "/CS", "2": "DO(IO1)", "3": "/WP(IO2)", "4": "GND", "5": "DI(IO0)", "6": "CLK",
               "7": "/HOLD(IO3)", "8": "VCC"}
C("100nF", "+3V3", "GND", 212.5, 108.6, 0)                      # flash
# RP2040 decoupling, two columns beside the chip
for k, (net, y) in enumerate([("+3V3", 117.5), ("+3V3", 119.5), ("+3V3", 121.5), ("+3V3", 123.5),
                              ("+1V1", 125.5)]):
    C("100nF", net, "GND", 206.5, y, 0)
for k, (net, y) in enumerate([("+3V3", 117.5), ("+3V3", 119.5), ("+1V1", 121.5), ("+3V3", 123.5),
                              ("+3V3", 125.5)]):
    C("100nF", net, "GND", 218.5, y, 0)
C("1uF", "+3V3", "GND", 204.5, 117.5, 90)                       # VREG_VIN
C("1uF", "+1V1", "GND", 204.5, 121.5, 90)                       # VREG_VOUT
C("100nF", "+3V3", "GND", 204.5, 125.5, 90)                     # ADC_AVDD
add("Y1", "XTAL", "X322512MSB4SI", 207, 130, 0, {"1": "XIN", "2": "GND", "3": "XOUT", "4": "GND"},
    sym=("12MHz", ["1", "2", "3", "4"]))
C("27pF", "XIN", "GND", 204, 133, 90)
C("27pF", "XOUT", "GND", 210, 133, 90)
R("1k", "XOUT_R", "XOUT", 211, 128.5, 0)
R("27R", "USB_DP_R", "USB_DP", 220.5, 117.5, 90)
R("27R", "USB_DN_R", "USB_DN", 222, 117.5, 90)
add("U1", "SOT23", "XC6206P332MR", 218.5, 130, 0, {"1": "GND", "2": "+3V3", "3": "+5V"},
    sym=("XC6206", ["1", "2", "3"]))
C("1uF", "+5V", "GND", 221.5, 128.5, 90)
C("10uF", "+3V3", "GND", 218.5, 133.5, 0, fp="C0603")
R("10k", "RUN", "+3V3", 216, 137.5, 90)
add("SW1", "TACT", "TS-1187A-B-A-B", 210, 138.5, 0, {"1": "BOOT_N", "2": "GND"}, sym=("BOOTSEL", ["1", "2"]))
R("1k", "QSPI_SS", "BOOT_N", 215, 133.5, 90)
add("SW2", "TACT", "TS-1187A-B-A-B", 210, 145, 0, {"1": "RUN", "2": "GND"}, sym=("RESET", ["1", "2"]))
add("J4", "HDR3", "SWD", 217.5, 141, 0, {"1": "SWCLK", "2": "GND", "3": "SWDIO"}, sym=("SWD", ["1", "2", "3"]),
    dnp=True, lcsc="")

# LED data level shifter (3.3 V -> 5 V)
add("U7", "SOT353", "74AHCT1G125", 220.5, 111.5, 0,
    {"1": "GND", "2": "LED_D3V3", "3": "GND", "4": "LED_D5V", "5": "+5V_LED"},
    sym=("74AHCT1G125", ["1", "2", "3", "4", "5"]))
C("100nF", "+5V_LED", "GND", 220.5, 114.5, 0)
R("330R", "LED_D5V", "LED_DIN_0", 223, 108.8, 90)

# ---------------- encoders ----------------
ENC_C = [(180, 30), (210, 30), (240, 30), (270, 30), (180, 68), (210, 68), (240, 68), (270, 68), (232, 132)]
for i, (cx, cy) in enumerate(ENC_C):
    n = i + 1
    add(f"ENC{n}", "ENC", "PEC11R-4220F-S0024", cx - 7.5, cy - 2.5, 0,
        {"A": f"ENC{n}_A", "B": f"ENC{n}_B", "C": "GND", "S1": f"ENC{n}_SW", "S2": "GND", "MP": "GND"},
        sym=("Encodeur", ["A", "B", "C", "S1", "S2"]), lcsc="C143797")

# ---------------- buttons ----------------
PAGE = ["ENGINE", "OSC", "FILTER", "ENV", "MOD", "LFO", "SEQ", "FX", "PRESET", "SHIFT"]
PAGE_X = [21.5 + 19 * k for k in range(10)]
PART_X = [180, 210, 240, 270]
for k, x in enumerate(PAGE_X):
    add(f"SW{10 + k + 1}", "CHOC", f"Choc {PAGE[k]}", x, 132, 0, {"1": f"SW_{PAGE[k]}", "2": "GND"},
        sym=(PAGE[k], ["1", "2"]), lcsc="C400229")
for k, x in enumerate(PART_X):
    add(f"SW{30 + k + 1}", "CHOC", f"Choc PART{k + 1}", x, 100, 0, {"1": f"SW_PART{k + 1}", "2": "GND"},
        sym=(f"PART {k + 1}", ["1", "2"]), lcsc="C400229")

# volume
add("RV1", "POT", "RK09L1140 A10k", 272 - 7.5, 132 + 2.5, 0, {"1": "GND", "2": "VOL", "3": "+3V3"},
    sym=("Volume", ["1", "2", "3"]), lcsc="C470448")
C("100nF", "VOL", "GND", 262, 126, 90)

# ---------------- shift registers + pull-ups + filters (band between encoder rows, y 44-54) ----------
INPUTS = [f"SW_{p}" for p in PAGE] + [f"SW_PART{k}" for k in range(1, 5)] + [f"ENC{n}_SW" for n in range(1, 10)] + ["+3V3"]
DPINS = ["11", "12", "13", "14", "3", "4", "5", "6"]   # A..H
SR_X = [195, 225, 255]
SER = ["SR_Q5", "SR_Q6", "GND"]
QH = ["SR_QH", "SR_Q5", "SR_Q6"]
for u in range(3):
    pads = {"1": "SR_LD", "2": "SR_CLK", "15": "GND", "16": "+3V3", "8": "GND", "9": QH[u], "10": SER[u]}
    for j, p in enumerate(DPINS):
        pads[p] = INPUTS[8 * u + j]
    add(f"U{4 + u}", "SOIC16", "74HC165D", SR_X[u], 49, 90, pads,
        sym=("74HC165", [str(i) for i in range(1, 17)]))
SR_NAMES = {"1": "SH/LD", "2": "CLK", "3": "E", "4": "F", "5": "G", "6": "H", "7": "/QH", "8": "GND", "9": "QH",
            "10": "SER", "11": "A", "12": "B", "13": "C", "14": "D", "15": "CLK_INH", "16": "VCC"}

slots = []
for x0, x1 in [(166.5, 188), (203, 217.5), (233, 247.5), (263, 284.5)]:
    x = x0
    while x <= x1:
        slots.append(x)
        x += 2.1
rows = [46.6, 51.4]
slot_iter = iter([(x, y) for y in rows for x in slots])
for u in range(3):
    x, y = next(slot_iter)
    C("100nF", "+3V3", "GND", x, y, 90)
for net in INPUTS[:-1]:
    x, y = next(slot_iter)
    R("10k", net, "+3V3", x, y, 90)
for n in range(1, 10):
    for ab in "AB":
        x, y = next(slot_iter)
        C("10nF", f"ENC{n}_{ab}", "GND", x, y, 90)

# ---------------- LEDs ----------------
led_pos = []   # (x, y, rot, cap_x, cap_y, cap_rot)
for (cx, cy) in ENC_C[:8]:
    for k in range(12):
        a = 15 + 30 * k
        r = math.radians(a)
        led_pos.append((cx + 11 * math.cos(r), cy + 11 * math.sin(r), 90 - a,
                        cx + 13.8 * math.cos(r), cy + 13.8 * math.sin(r), -(a + 90)))
for x in PART_X:
    led_pos.append((x, 89.5, 0, x + 2.8, 89.5, 90))
for x in PAGE_X:
    led_pos.append((x, 121.5, 0, x + 2.8, 121.5, 90))
for k, (x, y, rot, cx, cy, crot) in enumerate(led_pos):
    add(f"LED{k + 1}", "LED", "WS2812B-2020", x, y, rot,
        {"1": f"LED_DIN_{k + 1}" if k + 1 < len(led_pos) else "LED_DOUT_END", "2": "GND", "3": f"LED_DIN_{k}",
         "4": "+5V_LED"}, sym=("WS2812B", ["1", "2", "3", "4"]))
    if k >= 96 or k % 3 == 0:   # rings: one 100nF per 3 LEDs (keeps the ring routable)
        C("100nF", "+5V_LED", "GND", cx, cy, crot)
LED_NAMES = {"1": "DO", "2": "GND", "3": "DI", "4": "VDD"}

# ---------------- mounting holes ----------------
for k, (x, y) in enumerate([(8, 113), (8, 147), (163, 8), (292, 8), (292, 147), (163, 104)]):
    add(f"H{k + 1}", "HOLE", "M3", x, y, 0, {}, sym=None, lcsc="", dnp=True)

# board outline (L shape around the screen), panel coordinates
OUTLINE = [(159, 4), (296, 4), (296, 151), (4, 151), (4, 108), (159, 108)]

PIN_NAMES = {"U2": RP_NAMES, "U3": FLASH_NAMES, "U4": SR_NAMES, "U5": SR_NAMES, "U6": SR_NAMES}
for p in parts:
    if p["ref"].startswith("LED"):
        PIN_NAMES[p["ref"]] = LED_NAMES

if __name__ == "__main__":
    nets = sorted({n for p in parts for n in p["pads"].values()})
    print(len(parts), "parts,", len(nets), "nets")
# inner-plane outline (0.5 mm inside the board edge)
OUTLINE_IN = [(159.5, 4.5), (295.5, 4.5), (295.5, 150.5), (4.5, 150.5), (4.5, 108.5), (159.5, 108.5)]
