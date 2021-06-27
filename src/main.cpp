#include <Arduino.h>
#include <nts-1.h>

NTS1 nts1;

// enum { sw_0 = 0, sw_1, sw_2, sw_3, sw_4, sw_5, sw_6, sw_7, sw_8, sw_9, sw_count };
// const uint8_t sw_pins[sw_count] = {D34, D35, D36, D37, D38, D40, D41, D42, D46, D47};

// enum { sw_step0 = sw_0; };

// enum { led_0 = 0, led_1, led_2, led_3, led_4, led_5, led_6, led_7, led_count };
// const uint8_t led_pins[led_count] = {D16, D17, D18, D19, D22, D23, D24, D25};

void setup() {
    // init switches & LEDs
    // for (uint8_t i = 0; i < sw_count; ++i) {
    // pinMode(sw_pins[i], INPUT_PULLUP);
    // }
    // for (uint8_t i = 0; i < led_count; ++i) {
    //     uint8_t led_pin = led_pins[i];
    //     pinMode(led_pin, OUTPUT);
    // }

    pinMode(PC10, OUTPUT);

    nts1.init();

    // TEMP: turn on all LEDs
    // for (uint8_t i = led_0; i <= led_count; ++i) {
    // digitalWrite(led_pins[i], HIGH);
    // }
}

void loop() {
    nts1.idle();

    digitalWrite(PC10, HIGH);
    delay(300);

    digitalWrite(PC10, LOW);
    delay(800);
}