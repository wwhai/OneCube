#include "BeepControl.h"
#include <Arduino.h>

BeepControl::BeepControl(int beepPin) : beepPin(beepPin) {}

void BeepControl::init()
{
    pinMode(beepPin, OUTPUT);
}

void BeepControl::beep(int frequency, int duration)
{
    tone(beepPin, frequency, duration);
}

void BeepControl::stopBeep()
{
    noTone(beepPin);
    pinMode(beepPin, INPUT);
}
