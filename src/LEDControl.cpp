#include "LEDControl.h"
#include <Arduino.h>

LEDControl::LEDControl(int redPin, int greenPin, int yellowPin, int bluePin)
    : ledState(0)
{
    pins[0] = redPin;
    pins[1] = greenPin;
    pins[2] = yellowPin;
    pins[3] = bluePin;
    memset(blinks, 0, sizeof(blinks));
}

void LEDControl::init()
{
    for (int i = 0; i < 4; i++)
        pinMode(pins[i], OUTPUT);
    turnOff();
}

void LEDControl::setLed(uint8_t mask, bool on)
{
    for (int i = 0; i < 4; i++)
    {
        if (!(mask & (1u << i)))
            continue;
        blinks[i].active = false;
        digitalWrite(pins[i], on ? HIGH : LOW);
        if (on)
            ledState |= (uint8_t)(1u << i);
        else
            ledState &= (uint8_t)~(1u << i);
    }
}

void LEDControl::turnOff()
{
    for (int i = 0; i < 4; i++)
    {
        blinks[i].active = false;
        digitalWrite(pins[i], LOW);
    }
    ledState = 0;
}

void LEDControl::setBlink(uint8_t mask, uint16_t onMs, uint16_t offMs, uint8_t count)
{
    for (int i = 0; i < 4; i++)
    {
        if (!(mask & (1u << i)))
            continue;
        BlinkCtrl &b  = blinks[i];
        b.onMs        = onMs;
        b.offMs       = offMs;
        b.count       = count;
        b.remaining   = count;
        b.phase       = true;  // start in ON phase
        b.active      = true;
        b.lastMs      = millis();
        digitalWrite(pins[i], HIGH);
        ledState |= (uint8_t)(1u << i);
    }
}

void LEDControl::stopBlink(uint8_t mask)
{
    for (int i = 0; i < 4; i++)
    {
        if (!(mask & (1u << i)))
            continue;
        blinks[i].active = false;
        digitalWrite(pins[i], LOW);
        ledState &= (uint8_t)~(1u << i);
    }
}

uint8_t LEDControl::getState() const
{
    return ledState;
}

void LEDControl::update()
{
    unsigned long now = millis();
    for (int i = 0; i < 4; i++)
    {
        BlinkCtrl &b = blinks[i];
        if (!b.active)
            continue;

        uint16_t interval = b.phase ? b.onMs : b.offMs;
        if ((now - b.lastMs) < interval)
            continue;

        b.lastMs = now;
        b.phase  = !b.phase;

        if (b.phase)
        {
            // Entering ON phase – start of a new blink cycle
            digitalWrite(pins[i], HIGH);
            ledState |= (uint8_t)(1u << i);
        }
        else
        {
            // Entering OFF phase – end of a blink cycle
            digitalWrite(pins[i], LOW);
            ledState &= (uint8_t)~(1u << i);

            if (b.count != 0)
            {
                if (b.remaining > 0)
                    b.remaining--;
                if (b.remaining == 0)
                    b.active = false;
            }
        }
    }
}
