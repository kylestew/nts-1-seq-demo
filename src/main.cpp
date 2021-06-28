#include <Arduino.h>
#include <nts-1.h>

NTS1 nts1;

// -- HARDWARE UI pin definitiions and state ------------------------------------------
enum { sw_0 = 0, sw_1, sw_2, sw_3, sw_4, sw_5, sw_6, sw_7, sw_8, sw_9, sw_count };
const uint8_t sw_pins[sw_count] = {PC8, PC6, PC5, PA12, PA11, PB11, PB2, PB1, PC4, PF5};

enum {
    sw_step_0 = sw_0,
    sw_step_1 = sw_1,
    sw_step_2 = sw_2,
    sw_step_3 = sw_3,
    sw_step_4 = sw_4,
    sw_step_5 = sw_5,
    sw_step_6 = sw_6,
    sw_step_7 = sw_7,
    sw_play = sw_8,
    sw_shift = sw_9
};

enum { led0 = 0, led1, led2, led3, led4, led5, led6, led7, ledCount };
const uint8_t ledPins[ledCount] = {PC10, PC12, PF6, PF7, PA15, PB7, PC13, PC14};

typedef struct {
    HardwareTimer* timer;
} UIState;

UIState uiState = {.timer = NULL};

// -- SEQUENCER definitions and state -------------------------------------------------

#define SEQ_LENGTH 8
#define SEQ_TICKS_PER_STEP 100

typedef struct {
    uint32_t gates;
} SeqState;

SeqState seqState = {
    .gates = 0xFF,  // 1 bit per step, all on
};

// -- UI Scan/Control -----------------------------------------------------------------

void setStepLEDs(uint8_t mask) {
    for (uint8_t i = led0; i < ledCount; ++i) {
        // digitalWrite(ledPins[i], (mask & (1U << i)) ? HIGH : LOW);
        digitalWrite(ledPins[i], LOW);
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
        }

        last_sw_state = sw_state;
        last_sw_sample_us = now_us;
    }
}

void scanInterruptHandler(void) {
    unsigned long us = micros();
    scan_switches(us);
}

// -- SEQUENCER Runtime ---------------------------------------------------------------
// -- HARDWARE Timer Setup ------------------------------------------------------------

HardwareTimer* setupTimer(TIM_TypeDef* timer, uint32_t refreshHz, void (*interruptHandler)()) {
    HardwareTimer* tim = new HardwareTimer(timer);
    tim->setOverflow(refreshHz, HERTZ_FORMAT);
    tim->attachInterrupt(interruptHandler);
    return tim;
}

// -- MAIN ----------------------------------------------------------------------------

void setup() {
    // init switches & LEDs
    for (uint8_t i = 0; i < sw_count; ++i) {
        pinMode(sw_pins[i], INPUT_PULLUP);
    }
    for (uint8_t i = 0; i < ledCount; ++i) {
        pinMode(ledPins[i], OUTPUT);
    }

    nts1.init();

    // init UI state
    setStepLEDs(seqState.gates);

    // setup hardware timer for switch/pot scanning
    uiState.timer = setupTimer(TIM1, 120, scanInterruptHandler);

    uiState.timer->resume();
}

void loop() {
    nts1.idle();
    delay(10);
}