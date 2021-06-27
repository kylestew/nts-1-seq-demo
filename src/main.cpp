#include <Arduino.h>
#include <nts-1.h>

NTS1 nts1;

// -- HARDWARE UI pin definitiions and state ------------------------------------------
enum { sw0 = 0, sw1, sw2, sw3, sw4, sw5, sw6, sw7, sw8, sw9, swCount };
const uint8_t swPins[swCount] = {PC8, PC6, PC5, PA12, PA11, PB11, PB2, PB1, PC4, PF5};

// enum { sw_step0 = sw_0; };

enum { led0 = 0, led1, led2, led3, led4, led5, led6, led7, ledCount };
const uint8_t ledPins[ledCount] = {PC10, PC12, PF6, PF7, PA15, PB7, PC13, PC14};

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
        digitalWrite(ledPins[i], (mask & (1U << i)) ? HIGH : LOW);
    }
}

// -- SEQUENCER Runtime ---------------------------------------------------------------
// -- HARDWARE Timer Setup ------------------------------------------------------------
// -- MAIN ----------------------------------------------------------------------------

void setup() {
    // init switches & LEDs
    for (uint8_t i = 0; i < swCount; ++i) {
        pinMode(swPins[i], INPUT_PULLUP);
    }
    for (uint8_t i = 0; i < ledCount; ++i) {
        pinMode(ledPins[i], OUTPUT);
    }

    nts1.init();

    // setStepLEDs(seqState.gates);
}

void loop() { nts1.idle(); }