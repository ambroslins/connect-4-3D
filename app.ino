#include "FastLED.h"

#define SIZE_X 4
#define SIZE_Y 4
#define SIZE_Z 4

#define CHIPSET WS2812B

#define add_led_column(x, y, pin) \
    FastLED.addLeds<CHIPSET, pin>(leds[x][y], SIZE_Z);

CRGB leds[SIZE_X][SIZE_Y][SIZE_Z];

void setup() {
    add_led_column(0, 0, 0);
}

void loop() {}
