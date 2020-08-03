#include <NeoPixelBus.h>
#include <SimpleTimer.h>

#include "config.h"

enum Effect
{
    eStable,
    eSunrise,
    eColorLoop
};

extern SimpleTimer timer;
extern NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> strip;
extern RgbwColor stripLeds[NUM_LEDS];
extern Effect effect;
extern uint8_t red;
extern uint8_t green;
extern uint8_t blue;
extern uint8_t white;

void sunRise();
void startSunrise(int duration);
void startEffect();
void stopEffect();
