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
int aurora = NUM_LEDS;
int sunPhase = 256;
int wakeDelay = 256;
int fadeStep = 998;
int oldFadeStep = 0;
int currentAurora = 256;
int oldAurora = 0;
int currentSun = 256;
int oldSun = 0;
int sunFadeStep = 255;
int sunFadeStepTimerID = -1;
int fadeStepTimerID = -1;
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

void increaseFadeStep()
{
    if (fadeStep < 256)
    {
        fadeStep++;
    }
    if (sunPhase < 256)
    {
        fadeStepTimerID = timer.setTimeout((wakeDelay / NUM_LEDS / 2), increaseFadeStep);
    }
}

void increaseWhiteLevel()
{
    if (whiteLevel < 256)
    {
        whiteLevel++;
    }
    if (sunPhase < 256)
    {
        whiteLevelTimerID = timer.setTimeout(wakeDelay * 10, increaseWhiteLevel);
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
    if (newSunLeft >= 0 && newSunRight <= NUM_LEDS && sunPhase > 0)
    {
        int minRed = 127;
        int minGreen = 64;
        if (newSunRight - newSunLeft <= 2)
        {
            // first pixels, so no aurora on the background to fade from
            minRed = 0;
            minGreen = 0;
        }
        int redValue = map(sunFadeStep, 0, 256, minRed, 255);
        int greenValue = map(sunFadeStep, 0, 256, minGreen, 64);
        int whiteValue = map(sunFadeStep, 0, 256, 0, whiteLevel);
        stripLeds[newSunLeft] = RgbwColor(redValue, greenValue, 0, whiteValue);
        stripLeds[newSunRight] = RgbwColor(redValue, greenValue, 0, whiteValue);
    }
    for (int i = sunStart; i < sunStart + currentSun; i++)
    {
        stripLeds[i] = RgbwColor(255, 64, 0, whiteLevel);
    }
    oldSun = currentSun;
}

void drawAurora()
{
    currentAurora = map(sunPhase, 0, 256, 0, aurora);
    if (currentAurora % 2 != 0)
    {
        currentAurora--;
    }
    if (currentAurora != oldAurora)
    {
        fadeStep = 0;
    }
    int sunStart = (NUM_LEDS / 2) - (currentAurora / 2);
    int newAuroraLeft = sunStart - 1;
    int newAuroraRight = sunStart + currentAurora;
    if (newAuroraLeft >= 0 && newAuroraRight <= NUM_LEDS)
    {
        int redValue = map(fadeStep, 0, 256, whiteLevel / 2, 127);
        int greenValue = map(fadeStep, 0, 256, 0, 25);
        stripLeds[newAuroraRight] = RgbwColor(redValue, greenValue, 0, 0);
        stripLeds[newAuroraLeft] = RgbwColor(redValue, greenValue, 0, 0);
    }
    for (int i = sunStart; i < sunStart + currentAurora; i++)
    {
        stripLeds[i] = RgbwColor(127, 25, 0, 0);
    }
    oldFadeStep = fadeStep;
    oldAurora = currentAurora;
}

void drawAmbient()
{
    for (int i = 0; i < NUM_LEDS; i++)
    {
        stripLeds[i] = RgbwColor(whiteLevel / 2, 0, 0, 0);
    }
}

void sunrise()
{
    drawAmbient();
    drawAurora();
    drawSun();
}

void startSunrise(int duration)
{
    whiteLevel = 0;
    sunPhase = 0;
    fadeStep = 0;
    sunFadeStep = 0;
    wakeDelay = duration * 4;
    if (sunFadeStepTimerID != -1)
    {
        timer.deleteTimer(sunFadeStepTimerID);
    }
    if (fadeStepTimerID != -1)
    {
        timer.deleteTimer(fadeStepTimerID);
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
    increaseFadeStep();
    increaseSunFadeStep();
}
