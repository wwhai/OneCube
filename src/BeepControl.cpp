#include "BeepControl.h"
#include <Arduino.h>

BeepControl::BeepControl(int beepPin)
    : beepPin(beepPin), freq(0), onMs(0), offMs(0),
      count(0), remaining(0), phase(false), active(false), lastMs(0)
{}

void BeepControl::init()
{
    pinMode(beepPin, OUTPUT);
    noTone(beepPin);
}

void BeepControl::beepOnce(uint16_t frequency, uint16_t durationMs)
{
    beepPattern(frequency, durationMs, 0, 1);
}

void BeepControl::beepPattern(uint16_t frequency, uint16_t onMs_, uint16_t offMs_, uint8_t count_)
{
    if (count_ == 0)
        return;  // 0 is not valid here; use stop() to silence

    noTone(beepPin);
    freq      = frequency;
    onMs      = onMs_;
    offMs     = offMs_;
    count     = count_;
    remaining = count_;
    phase     = true;     // start in ON phase
    active    = true;
    lastMs    = millis();
    tone(beepPin, freq);
}

void BeepControl::stop()
{
    active = false;
    noTone(beepPin);
    pinMode(beepPin, OUTPUT);  // restore OUTPUT after noTone
}

bool BeepControl::isActive() const
{
    return active;
}

void BeepControl::update()
{
    if (!active)
        return;

    unsigned long now      = millis();
    uint16_t      interval = phase ? onMs : offMs;

    if ((now - lastMs) < interval)
        return;

    lastMs = now;
    phase  = !phase;

    if (phase)
    {
        // Entering ON phase
        tone(beepPin, freq);
    }
    else
    {
        // Entering OFF phase – end of one cycle
        noTone(beepPin);
        pinMode(beepPin, OUTPUT);

        if (remaining > 0)
            remaining--;
        if (remaining == 0)
            active = false;
    }
}
