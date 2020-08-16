/////////////////////////////////////////////////////////////////////////////////////////////////////
// Sunrise effect. Mostly taken from:                                                              //
// https://github.com/thehookup/RGBW-Sunrise-Animation-Neopixel-/blob/master/Sunrise_CONFIGURE.ino //
/////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#include <NeoPixelBus.h>
#include <SimpleTimer.h>

#include "common.h"

int whiteLevel = 255;
int sun = (SUNSIZE * NUM_LEDS) / 100;
int sunPhase = 256;
int wakeDelay = 256;
int oldFadeStep = 0;
int currentAurora = 256;
int currentSun = 256;
int oldSun = 0;
int sunFadeStep = 255;
int sunFadeStepTimerID = -1;
int whiteLevelTimerID = -1;
int sunPhaseTimerID = -1;

void increaseSunFadeStep();
void increaseFadeStep();
void increaseWhiteLevel();
void increaseSunPhase();

void increaseSunPhase()
{
    if (sunPhase < 256)
    {
        sunPhase++;
        Serial.print("sunPhase: ");
        Serial.println(sunPhase);
        sunPhaseTimerID = timer.setTimeout(wakeDelay, increaseSunPhase);
    }
}

void increaseSunFadeStep()
{
    if (sunFadeStep < 256)
    {
        sunFadeStep++;
    }
    if (sunPhase < 256)
    {
        sunFadeStepTimerID = timer.setTimeout((wakeDelay / NUM_LEDS / 2), increaseSunFadeStep);
    }
}

void increaseWhiteLevel()
{
    if (whiteLevel < 256)
    {
        whiteLevel++;
    }
    if (sunPhase < 128)
    {
        whiteLevelTimerID = timer.setTimeout(wakeDelay * 10, increaseWhiteLevel);
    }
    else if (sunPhase < 256)
    {
        whiteLevelTimerID = timer.setTimeout(wakeDelay * 2, increaseWhiteLevel);
    }
}

void drawAurora(int sunLeft, int SunRight)
{
    RgbwColor color = RgbwColor(1, 0, 0, 0);
    for (int i = 0; i < sunLeft; i++)
    {
        stripLeds[i] = color;
    }
    for (int i = SunRight + 1; i <= NUM_LEDS; i++)
    {
        stripLeds[i] = color;
    }
}

void drawSun()
{
    currentSun = map(sunPhase, 0, 256, 0, sun);
    if (currentSun % 2 != 0)
    {
        currentSun--;
    }
    if (currentSun != oldSun)
    {
        sunFadeStep = 0;
    }

    int sunStart = (NUM_LEDS / 2) - (currentSun / 2);
    int newSunLeft = sunStart - 1;
    int newSunRight = sunStart + currentSun;
    int maxRed = map(sunPhase, 0, 256, 255, -20);
    if (maxRed < 0) {
        maxRed = 0;
    }
    int maxGreen = map(sunPhase, 0, 256, 64, 0);
    if (newSunLeft >= 0 && newSunRight <= NUM_LEDS && sunPhase > 0)
    {
        int redValue = map(sunFadeStep, 0, 256, 1, maxRed);
        int greenValue = map(sunFadeStep, 0, 256, 0, maxGreen);
        int whiteValue = map(sunFadeStep, 0, 256, 0, whiteLevel);
        stripLeds[newSunLeft] = RgbwColor(redValue, greenValue, 0, whiteValue);
        stripLeds[newSunRight] = RgbwColor(redValue, greenValue, 0, whiteValue);
    }
    drawAurora(newSunLeft, newSunRight);

    RgbwColor color = RgbwColor(maxRed, maxGreen, 0, whiteLevel);
    for (int i = sunStart; i < sunStart + currentSun; i++)
    {
        stripLeds[i] = color;
    }
    oldSun = currentSun;
}

void sunrise()
{
    drawSun();
}

void startSunrise(int duration)
{
    whiteLevel = 0;
    sunPhase = 0;
    sunFadeStep = 0;
    wakeDelay = duration * 4;
    if (sunFadeStepTimerID != -1)
    {
        timer.deleteTimer(sunFadeStepTimerID);
    }
    if (whiteLevelTimerID != -1)
    {
        timer.deleteTimer(whiteLevelTimerID);
    }
    if (sunPhaseTimerID != -1)
    {
        timer.deleteTimer(sunPhaseTimerID);
    }
    increaseSunPhase();
    increaseWhiteLevel();
    increaseSunFadeStep();
}
