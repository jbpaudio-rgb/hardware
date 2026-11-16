// Octavio Synth front panel firmware (RP2040).
//
// USB-MIDI device "Octavio Facade". Everything runs on MIDI channel 16; the protocol is in PROTOCOLE.md.
//   9 encoders (PEC11R, 24 detents)  -> CC 16..24 relative (1..63 up, 127..65 down)
//   9 encoder pushes                 -> notes 0..8
//   10 page buttons / 4 part buttons -> notes 16..25 / 32..35
//   volume pot                       -> CC 7 (0..127)
//   host -> panel: CC 16..23 = ring values, SysEx F0 7D 4F ... for colours and single LEDs
//
// Pins (see the board): encoder n A/B = GPIO 2n / 2n+1 (n = 0..8), 74HC165 chain CLK 18, SH/LD 19, QH 20,
// WS2812B data GPIO 22 (through a 5 V buffer), volume on GPIO 26 (ADC0).
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "tusb.h"
#include "ws2812.pio.h"

#define FW_MAJOR 1
#define FW_MINOR 1

#define N_ENC 9
#define PIN_SR_CLK 18
#define PIN_SR_LD 19
#define PIN_SR_QH 20
#define PIN_LED 22
#define PIN_VOL 26
#define PIN_PWR 28        // -> Q1 (2N7002): pulls the Pi 5's power-button pads (its J2) low, to switch it on
#define ADC_VOL 0
#define N_LED 110
#define N_RING 8
#define LED_PART0 96
#define LED_PAGE0 100
#define CH 15                    // MIDI channel 16
#define ENC_DIR 1                // set to -1 if the knobs turn the wrong way
#define LED_LIMIT_MA 1200        // keep the LED supply well under the 2 A fuse on the main board
#define MA_PER_FULL_CHANNEL 12   // WS2812B-2020 at 255, per colour channel (datasheet order of magnitude)

// ---------------------------------------------------------------- MIDI out
static void midi_send(const uint8_t *b, uint32_t n) {
    if (tud_midi_mounted()) tud_midi_stream_write(0, b, n);
}
static void send_cc(uint8_t cc, uint8_t v) {
    uint8_t m[3] = {(uint8_t)(0xB0 | CH), cc, (uint8_t)(v & 0x7F)};
    midi_send(m, 3);
}
static void send_note(uint8_t note, bool on) {
    uint8_t m[3] = {(uint8_t)((on ? 0x90 : 0x80) | CH), note, (uint8_t)(on ? 127 : 0)};
    midi_send(m, 3);
}
static void send_identity(void) {
    uint8_t m[] = {0xF0, 0x7D, 0x4F, 0x02, FW_MAJOR, FW_MINOR, N_ENC, N_RING, 0xF7};
    midi_send(m, sizeof m);
}

// ---------------------------------------------------------------- encoders (sampled at 4 kHz in an IRQ)
static const int8_t QDEC[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
static volatile int32_t enc_detents[N_ENC];
static uint8_t enc_state[N_ENC];
static int8_t enc_acc[N_ENC];

static bool enc_sample(struct repeating_timer *t) {
    (void)t;
    uint32_t all = gpio_get_all();
    for (int i = 0; i < N_ENC; i++) {
        uint8_t cur = (all >> (2 * i)) & 3;
        uint8_t idx = (uint8_t)((enc_state[i] << 2) | cur);
        enc_state[i] = cur;
        enc_acc[i] += QDEC[idx];
        if (enc_acc[i] >= 4) { enc_acc[i] = 0; enc_detents[i]++; }
        else if (enc_acc[i] <= -4) { enc_acc[i] = 0; enc_detents[i]--; }
    }
    return true;
}

static int32_t enc_sent[N_ENC];
static uint64_t enc_last_us[N_ENC];

static void encoders_poll(uint64_t now) {
    for (int i = 0; i < N_ENC; i++) {
        int32_t d = enc_detents[i] - enc_sent[i];
        if (!d) continue;
        enc_sent[i] += d;
        int step = d * ENC_DIR;
        uint64_t dt = now - enc_last_us[i];   // acceleration when turned fast
        enc_last_us[i] = now;
        if (dt < 15000) step *= 4;
        else if (dt < 40000) step *= 2;
        if (step > 63) step = 63;
        if (step < -63) step = -63;
        send_cc((uint8_t)(16 + i), (uint8_t)(step > 0 ? step : 128 + step));
    }
}

// ---------------------------------------------------------------- buttons (3 x 74HC165, 1 kHz, debounced)
// chain order after the parallel load: U4 H..A, U5 H..A, U6 H..A. Inputs (board design.py):
// 0..9 page buttons, 10..13 part buttons, 14..22 encoder pushes, 23 tied to 3V3 (chain check)
static uint32_t btn_state = 0x00FFFFFF;   // 1 = released (pull-ups)
// DÉFILER (encoder 9's push) held 2 s: the machine on / off. Pi running (USB host there): note 9, and octavio-box
// keeps the session and shuts down. Pi off: a 300 ms pulse on its power button wakes it (the panel stays powered
// from the 5 V header through D3 while the Pi is halted)
#define LONG_PRESS_US 2000000
static uint64_t k9_down = 0, pwr_until = 0, host_gone = 0;
static bool k9_fired = false;
static uint8_t btn_count[24];

static uint32_t sr_read(void) {
    gpio_put(PIN_SR_LD, 0);
    busy_wait_us_32(1);
    gpio_put(PIN_SR_LD, 1);
    busy_wait_us_32(1);
    uint32_t v = 0;
    for (int k = 0; k < 24; k++) {
        int input = 8 * (k / 8) + (7 - k % 8);
        if (gpio_get(PIN_SR_QH)) v |= 1u << input;
        gpio_put(PIN_SR_CLK, 1);
        busy_wait_us_32(1);
        gpio_put(PIN_SR_CLK, 0);
        busy_wait_us_32(1);
    }
    return v;
}

static uint8_t input_note(int input) {
    if (input < 10) return (uint8_t)(16 + input);          // pages
    if (input < 14) return (uint8_t)(32 + input - 10);     // parts
    return (uint8_t)(input - 14);                           // encoder pushes
}

static void buttons_poll(void) {
    uint32_t raw = sr_read();
    for (int i = 0; i < 23; i++) {
        uint32_t bit = 1u << i;
        if ((raw & bit) == (btn_state & bit)) { btn_count[i] = 0; continue; }
        if (++btn_count[i] < 5) continue;                   // 5 ms stable
        btn_count[i] = 0;
        btn_state ^= bit;
        const bool down = !(btn_state & bit);
        if (i == 22) k9_down = down ? time_us_64() : 0, k9_fired = false;   // DÉFILER: timed for the long press
        send_note(input_note(i), down);
    }
}

// ---------------------------------------------------------------- volume pot
static int32_t vol_filt = -1;
static int vol_sent = -1;

static void volume_poll(void) {
    adc_select_input(ADC_VOL);
    int32_t raw = adc_read() << 4;                          // 16-bit scale
    vol_filt = vol_filt < 0 ? raw : vol_filt + ((raw - vol_filt) >> 3);
    int v = vol_filt >> 9;                                  // 0..127
    if (v != vol_sent) {
        int centre = vol_sent * 512 + 256;
        if (vol_sent < 0 || abs(vol_filt - centre) > 400) {  // hysteresis
            vol_sent = v;
            send_cc(7, (uint8_t)v);
        }
    }
}

// ---------------------------------------------------------------- LEDs
typedef struct { uint8_t r, g, b; } rgb_t;
typedef struct { uint8_t value, mode; rgb_t col; } ring_t;   // mode 0 arc, 1 dot, 2 bipolar, 3 off

static ring_t rings[N_RING];
static rgb_t over[N_LED];          // explicit colours from the host
static bool over_on[N_LED];
static uint8_t brightness = 48;    // 0..127
static bool dark = false;          // the Pi is off (no USB host): all LEDs out
static bool dirty = true;
static uint32_t frame[N_LED];
static int dma_ch;
static uint8_t gamma8[256];

// ring LED k sits at 15 + 30k degrees clockwise from 3 o'clock: the arc runs 7:30 -> 4:30 clockwise
static const uint8_t ARC[10] = {4, 5, 6, 7, 8, 9, 10, 11, 0, 1};
static const uint8_t BOTTOM[2] = {2, 3};

static float clampf(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

static void ring_levels(const ring_t *r, float lv[10]) {
    float v = r->value / 127.0f;
    for (int i = 0; i < 10; i++) lv[i] = 0;
    if (r->mode == 0) {
        float l = v * 10;
        for (int i = 0; i < 10; i++) lv[i] = clampf(l - i);
    } else if (r->mode == 1) {
        float p = v * 9;
        int a = (int)p;
        float f = p - a;
        lv[a] = 1 - f;
        if (a < 9) lv[a + 1] = f;
    } else if (r->mode == 2) {
        float d = (r->value - 64) / 63.0f * 5;
        if (d >= 0) for (int i = 5; i < 10; i++) lv[i] = clampf(d - (i - 5));
        else for (int i = 4; i >= 0; i--) lv[i] = clampf(-d - (4 - i));
        if (r->value == 64) lv[4] = lv[5] = 0.3f;
    }
}

static uint8_t scale8(uint8_t c, float k) { return (uint8_t)(c * k + 0.5f); }

static void render(void) {
    static rgb_t px[N_LED];
    memset(px, 0, sizeof px);
    for (int r = 0; r < N_RING; r++) {
        const ring_t *rg = &rings[r];
        if (rg->mode == 3) continue;
        float lv[10];
        ring_levels(rg, lv);
        for (int i = 0; i < 10; i++) {
            float k = 0.06f + 0.94f * lv[i];                 // dim background so the ring stays visible
            rgb_t *p = &px[r * 12 + ARC[i]];
            p->r = scale8(rg->col.r, k); p->g = scale8(rg->col.g, k); p->b = scale8(rg->col.b, k);
        }
        for (int i = 0; i < 2; i++) {
            rgb_t *p = &px[r * 12 + BOTTOM[i]];
            p->r = scale8(rg->col.r, 0.15f); p->g = scale8(rg->col.g, 0.15f); p->b = scale8(rg->col.b, 0.15f);
        }
    }
    for (int i = 0; i < N_LED; i++)
        if (over_on[i]) px[i] = over[i];
    // global brightness, gamma, then current limit
    float kb = dark ? 0.f : brightness / 127.0f;   // dark: the Pi is off, the panel waits for DÉFILER
    uint32_t sum = 0;
    static uint8_t out[N_LED][3];
    for (int i = 0; i < N_LED; i++) {
        out[i][0] = gamma8[scale8(px[i].r, kb)];
        out[i][1] = gamma8[scale8(px[i].g, kb)];
        out[i][2] = gamma8[scale8(px[i].b, kb)];
        sum += out[i][0] + out[i][1] + out[i][2];
    }
    float ma = sum / 255.0f * MA_PER_FULL_CHANNEL;
    float lim = ma > LED_LIMIT_MA ? LED_LIMIT_MA / ma : 1.0f;
    for (int i = 0; i < N_LED; i++) {
        uint32_t r = scale8(out[i][0], lim), g = scale8(out[i][1], lim), b = scale8(out[i][2], lim);
        frame[i] = (g << 24) | (r << 16) | (b << 8);
    }
}

static void show(void) {
    if (dma_channel_is_busy(dma_ch)) return;
    render();
    dma_channel_set_read_addr(dma_ch, frame, false);
    dma_channel_set_trans_count(dma_ch, N_LED, true);
}

static void leds_init(void) {
    for (int i = 0; i < 256; i++) {
        float x = i / 255.0f;
        gamma8[i] = (uint8_t)(x * x * (0.25f + 0.75f * x) * 255 + 0.5f);   // ~gamma 2.6, smooth at the bottom
    }
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, PIN_LED, 800000);
    dma_ch = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_ch);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));
    dma_channel_configure(dma_ch, &c, &pio->txf[sm], frame, N_LED, false);
    const rgb_t idle[N_RING] = {{127, 40, 0}, {127, 40, 0}, {127, 40, 0}, {127, 40, 0},
                                {0, 70, 127}, {0, 70, 127}, {0, 70, 127}, {0, 70, 127}};
    for (int r = 0; r < N_RING; r++) {
        rings[r].value = 0;
        rings[r].mode = 0;
        rings[r].col = (rgb_t){(uint8_t)(idle[r].r * 2), (uint8_t)(idle[r].g * 2), (uint8_t)(idle[r].b * 2)};
    }
}

static void boot_animation(void) {
    for (int step = 0; step <= 127; step += 4) {
        for (int r = 0; r < N_RING; r++) rings[r].value = (uint8_t)step;
        show();
        sleep_ms(12);
    }
    for (int r = 0; r < N_RING; r++) rings[r].value = 0;
    show();
}

// ---------------------------------------------------------------- the machine on / off
static void power_poll(uint64_t now) {
    const bool host = tud_mounted();
    if (k9_down && !k9_fired && now - k9_down >= LONG_PRESS_US) {
        k9_fired = true;
        if (host) { send_note(9, true); send_note(9, false); }          // octavio-box shuts the Pi down
        else { gpio_put(PIN_PWR, 1); pwr_until = now + 300000; dark = false; boot_animation(); }   // wake the Pi
    }
    if (pwr_until && now >= pwr_until) { gpio_put(PIN_PWR, 0); pwr_until = 0; }
    // no host for 5 s (the Pi halted, or not started yet): LEDs out; back on as soon as it is there
    if (host) { host_gone = 0; if (dark) { dark = false; dirty = true; } }
    else if (!host_gone) host_gone = now;
    else if (!dark && now - host_gone > 5000000) { dark = true; dirty = true; }
}

// ---------------------------------------------------------------- MIDI in
static uint8_t sx[600];
static int sx_len = -1;            // -1 = not in a SysEx
static uint8_t run_status, data[2];
static int ndata;

static uint8_t c7to8(uint8_t c) { return (uint8_t)(c * 2 + (c >> 6)); }

static void handle_sysex(const uint8_t *m, int n) {
    // m = F0 7D 4F cmd ... F7
    if (n < 5 || m[1] != 0x7D || m[2] != 0x4F) return;
    const uint8_t *p = m + 4;
    int len = n - 5;
    switch (m[3]) {
    case 0x01: send_identity(); break;
    case 0x10:                                     // ring, value, r, g, b, mode
        if (len >= 6 && p[0] < N_RING) {
            ring_t *r = &rings[p[0]];
            r->value = p[1];
            r->col = (rgb_t){c7to8(p[2]), c7to8(p[3]), c7to8(p[4])};
            r->mode = p[5] & 3;
            dirty = true;
        }
        break;
    case 0x11:                                     // led, r, g, b
        if (len >= 4 && p[0] < N_LED) {
            over[p[0]] = (rgb_t){c7to8(p[1]), c7to8(p[2]), c7to8(p[3])};
            over_on[p[0]] = true;
            dirty = true;
        }
        break;
    case 0x12:                                     // start, count, count x (r, g, b)
        if (len >= 2) {
            int s = p[0], cnt = p[1];
            for (int i = 0; i < cnt && 2 + 3 * i + 2 < len && s + i < N_LED; i++) {
                over[s + i] = (rgb_t){c7to8(p[2 + 3 * i]), c7to8(p[3 + 3 * i]), c7to8(p[4 + 3 * i])};
                over_on[s + i] = true;
            }
            dirty = true;
        }
        break;
    case 0x13:                                     // brightness 0..127
        if (len >= 1) { brightness = p[0]; dirty = true; }
        break;
    case 0x14:                                     // release LEDs set with 0x11/0x12
        memset(over_on, 0, sizeof over_on);
        dirty = true;
        break;
    }
}

static void handle_channel(uint8_t st, const uint8_t *d) {
    if ((st & 0x0F) != CH) return;
    if ((st & 0xF0) == 0xB0 && d[0] >= 16 && d[0] < 16 + N_RING) {
        rings[d[0] - 16].value = d[1];
        dirty = true;
    }
}

static void midi_in_poll(void) {
    uint8_t buf[64];
    uint32_t n;
    while (tud_midi_available() && (n = tud_midi_stream_read(buf, sizeof buf)) > 0) {
        for (uint32_t i = 0; i < n; i++) {
            uint8_t b = buf[i];
            if (b >= 0xF8) continue;                         // realtime
            if (b == 0xF0) { sx_len = 0; sx[sx_len++] = b; continue; }
            if (b == 0xF7) {
                if (sx_len > 0 && sx_len < (int)sizeof sx) { sx[sx_len++] = b; handle_sysex(sx, sx_len); }
                sx_len = -1;
                continue;
            }
            if (b & 0x80) { sx_len = -1; run_status = b; ndata = 0; continue; }
            if (sx_len >= 0) { if (sx_len < (int)sizeof sx - 1) sx[sx_len++] = b; continue; }
            if (!run_status) continue;
            data[ndata++] = b;
            int need = ((run_status & 0xF0) == 0xC0 || (run_status & 0xF0) == 0xD0) ? 1 : 2;
            if (ndata >= need) { handle_channel(run_status, data); ndata = 0; }
        }
    }
}

void tud_mount_cb(void) { send_identity(); }

// ---------------------------------------------------------------- main
int main(void) {
    for (int p = 0; p < 2 * N_ENC; p++) {
        gpio_init(p);
        gpio_set_dir(p, GPIO_IN);
        gpio_pull_up(p);                                     // encoder A/B (10 nF filter caps on the board)
    }
    gpio_init(PIN_PWR); gpio_set_dir(PIN_PWR, GPIO_OUT); gpio_put(PIN_PWR, 0);
    gpio_init(PIN_SR_CLK); gpio_set_dir(PIN_SR_CLK, GPIO_OUT); gpio_put(PIN_SR_CLK, 0);
    gpio_init(PIN_SR_LD); gpio_set_dir(PIN_SR_LD, GPIO_OUT); gpio_put(PIN_SR_LD, 1);
    gpio_init(PIN_SR_QH); gpio_set_dir(PIN_SR_QH, GPIO_IN);
    adc_init();
    adc_gpio_init(PIN_VOL);

    sleep_ms(5);
    uint32_t all = gpio_get_all();
    for (int i = 0; i < N_ENC; i++) enc_state[i] = (all >> (2 * i)) & 3;
    static struct repeating_timer enc_timer;
    add_repeating_timer_us(-250, enc_sample, NULL, &enc_timer);

    leds_init();
    boot_animation();
    tusb_init();

    uint64_t next_scan = 0, next_vol = 0, last_show = 0;
    while (true) {
        tud_task();
        uint64_t now = time_us_64();
        if (now >= next_scan) { next_scan = now + 1000; buttons_poll(); }
        if (now >= next_vol) { next_vol = now + 5000; volume_poll(); }
        encoders_poll(now);
        midi_in_poll();
        power_poll(now);
        if (dirty && now - last_show > 16000) {
            if (!dma_channel_is_busy(dma_ch)) { show(); dirty = false; last_show = now; }
        }
    }
}
