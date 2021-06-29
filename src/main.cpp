#include <Arduino.h>
#include <nts-1.h>

NTS1 nts1;

// -- HARDWARE UI pin definitiions and state ------------------------------------------
enum { sw_0 = 0, sw_1, sw_2, sw_3, sw_4, sw_5, sw_6, sw_7, sw_8, sw_9, sw_count };
const uint8_t sw_pins[sw_count] = {PC8, PC6, PC5, PA12, PA11, PB11, PB2, PB1, PC4, PF5};

enum {
    sw_step0 = sw_0,
    sw_step1 = sw_1,
    sw_step2 = sw_2,
    sw_step3 = sw_3,
    sw_step4 = sw_4,
    sw_step5 = sw_5,
    sw_step6 = sw_6,
    sw_step7 = sw_7,
    sw_play = sw_8,
    sw_shift = sw_9
};

enum { led0 = 0, led1, led2, led3, led4, led5, led6, led7, ledCount };
const uint8_t g_led_pins[ledCount] = {PC10, PC12, PF6, PF7, PA15, PB7, PC13, PC14};

typedef struct {
    HardwareTimer* timer;
} UIState;

UIState g_ui_state = {.timer = NULL};

// -- SEQUENCER definitions and state -------------------------------------------------

#define k_seq_length 8
#define k_seq_ticks_per_step 100

enum { k_seq_flag_reset = 1U << 0 };

typedef struct {
    HardwareTimer* timer;
    uint32_t last_tick_us;
    uint32_t ticks;
    uint8_t step;
    uint8_t note;
    uint8_t flags;
    uint32_t gates;
    uint32_t tempo;
    uint8_t notes[k_seq_length];
    bool is_playing;
} seq_state_t;

seq_state_t g_seq_state = {.timer = NULL,
                           .last_tick_us = 0,
                           .ticks = 0xFF,  // invalid
                           .step = 0xFF,   // invalid
                           .note = 0xFF,   // invalid
                           .flags = 0x00,  // none
                           .gates = 0xFF,  // 1 bit per step, all on
                           .tempo = 1200,  // 120.0 x 10
                           .notes = {0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42},
                           .is_playing = false};

// -- UI Scan/Control -----------------------------------------------------------------

void set_step_leds(uint8_t mask) {
    for (uint8_t i = led0; i < ledCount; ++i) {
        digitalWrite(g_led_pins[i], (mask & (1U << i)) ? HIGH : LOW);
    }
}

void scan_switches(unsigned long now_us) {
    static uint32_t last_sw_sample_us;
    static uint32_t last_sw_state = 0;
    static uint8_t sw_chatter[sw_count] = {0};
    uint32_t sw_events = 0;

    if (now_us - last_sw_sample_us) {
        uint32_t sw_state = 0;

        for (uint8_t i = 0; i < sw_count; ++i) {
            const uint32_t val = digitalRead(sw_pins[i]);
            const uint32_t lastVal = ((last_sw_state >> i) & 0x1);
            if (val != lastVal) {
                if (++sw_chatter[i] > 6) {
                    // toggle value and reset chatter counter
                    sw_state |= val << i;
                    sw_chatter[i] = 0;
                } else {
                    // keep current value
                    sw_state |= lastVal << i;
                }
            } else {
                sw_chatter[i] = 0;
                sw_state |= lastVal << i;
            }
        }

        if (last_sw_state != sw_state) {
            // change detected

            sw_events = sw_state ^ last_sw_state;

            // check for play switch event
            static const uint32_t k_play_sw_mask = (1U << sw_play);
            if (sw_events & k_play_sw_mask) {
                if ((~sw_state) & k_play_sw_mask) {
                    // pressed down
                    // reset sequencer
                    g_seq_state.flags |= k_seq_flag_reset;
                    // toggle play state
                    g_seq_state.is_playing = !g_seq_state.is_playing;
                }
            }

            // check for shift switch event
            //...

            // check for step switch events
            static const uint32_t k_step_sw_mask = ((1U << sw_8) - 1);
            const uint32_t step_sw_events = sw_events & k_step_sw_mask;
            if (step_sw_events) {
                const uint32_t new_presses = (~sw_state) & step_sw_events;
                const uint32_t released = sw_state & step_sw_events;

                // set/unset sequencer gates
                g_seq_state.gates ^= new_presses >> sw_step0;
                set_step_leds(g_seq_state.gates);
            }
        }

        last_sw_state = sw_state;
        last_sw_sample_us = now_us;
    }
}

void scan_interrupt_handler(void) {
    unsigned long us = micros();
    scan_switches(us);
}

// -- SEQUENCER Runtime ---------------------------------------------------------------

void seq_interrupt_handler() {
    uint32_t now_us = micros();

    if (g_seq_state.flags & k_seq_flag_reset) {
        if (g_seq_state.note != 0xFF) {
            // there's a pending note on, send note off
            nts1.noteOff(g_seq_state.note);
            const uint32_t cur_step = g_seq_state.step;
            const uint8_t highlow =
                (g_seq_state.gates & (1U << cur_step)) ? HIGH : LOW;  // revert LEDs
            digitalWrite(g_led_pins[cur_step], highlow);
        }
        // reset sequencer
        g_seq_state.ticks = 0xFF;
        g_seq_state.step = 0xFF;
        g_seq_state.note = 0xFF;
        g_seq_state.last_tick_us = now_us;
        g_seq_state.flags &= ~k_seq_flag_reset;
    }

    if (!g_seq_state.is_playing) {
        return;
    }

    // have we counted another tick?
    const uint32_t us_per_tick = 600000000UL / (4 * g_seq_state.tempo * 100);
    const uint32_t tick_delta_us = now_us - g_seq_state.last_tick_us;
    if (tick_delta_us < us_per_tick) {
        // still counting up current tick
        return;
    }

    // increment tick
    ++g_seq_state.ticks;

    // set adjusted reference time for next tick (keeping any extra time)
    g_seq_state.last_tick_us = now_us - (tick_delta_us - us_per_tick);

    uint32_t cur_step = g_seq_state.step;

    if (g_seq_state.ticks >= k_seq_ticks_per_step) {
        // increment step
        cur_step = (cur_step + 1) % k_seq_length;
        g_seq_state.ticks = 0;

        const uint8_t note = g_seq_state.notes[cur_step];

        if (g_seq_state.gates & (1U << cur_step)) {
            // send note on event to NTS-1
            nts1.noteOn(note, 0x7F);
            digitalWrite(g_led_pins[cur_step], LOW);
        } else {
            digitalWrite(g_led_pins[cur_step], HIGH);
        }

        g_seq_state.step = cur_step;
        g_seq_state.note = note;

    } else if (g_seq_state.ticks >= (k_seq_ticks_per_step >> 1)) {
        // half way through step
        if (g_seq_state.note != 0xFF) {
            // send note off event to NTS-1
            nts1.noteOff(g_seq_state.note);
            g_seq_state.note = 0xFF;

            // revert LED
            uint32_t cur_step = g_seq_state.step;
            const uint8_t highlow = (g_seq_state.gates & (1U << cur_step)) ? HIGH : LOW;
            digitalWrite(g_led_pins[cur_step], highlow);
        }
    }
}

// -- HARDWARE Timer Setup ------------------------------------------------------------

HardwareTimer* setup_timer(TIM_TypeDef* timer, uint32_t refresh_hz, void (*interrupt_handler)()) {
    HardwareTimer* tim = new HardwareTimer(timer);
    tim->setOverflow(refresh_hz, HERTZ_FORMAT);
    tim->attachInterrupt(interrupt_handler);
    return tim;
}

// -- MAIN ----------------------------------------------------------------------------

void setup() {
    // init switches & LEDs
    for (uint8_t i = 0; i < sw_count; ++i) {
        pinMode(sw_pins[i], INPUT_PULLUP);
    }
    for (uint8_t i = 0; i < ledCount; ++i) {
        pinMode(g_led_pins[i], OUTPUT);
    }

    nts1.init();

    // init UI state
    set_step_leds(g_seq_state.gates);

    // setup hardware timer for switch/pot scanning
    g_ui_state.timer = setup_timer(TIM1, 200, scan_interrupt_handler);

    // setup hardware timer for sequencer
    g_seq_state.timer = setup_timer(TIM3, 200, seq_interrupt_handler);

    // start timers
    g_ui_state.timer->resume();
    g_seq_state.timer->resume();
}

void loop() { nts1.idle(); }