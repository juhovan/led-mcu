#include <NeoPixelBus.h>
#include <SimpleTimer.h>

#include "config.h"

enum Effect
{
    eStable,
    eGradient,
    eSunrise,
    eColorLoop
};

extern SimpleTimer timer;
extern RgbwColor stripLeds[NUM_LEDS];
extern Effect effect;
extern uint8_t red;
extern uint8_t green;
extern uint8_t blue;
extern uint8_t white;
extern char gradientMode;
extern int gradientExtent;
extern int sunriseDuration;

void sunrise();
void startEffect(Effect e);
void startSunrise(int duration);
void stopEffect();
