#include "LEDControl.h"
#include <Arduino.h>

LEDControl::LEDControl(int redPin, int greenPin, int yellowPin, int bluePin)
    : redPin(redPin), greenPin(greenPin), yellowPin(yellowPin), bluePin(bluePin) {}

void LEDControl::init()
{
    pinMode(redPin, OUTPUT);
    pinMode(greenPin, OUTPUT);
    pinMode(yellowPin, OUTPUT);
    pinMode(bluePin, OUTPUT);
}

void LEDControl::setColor(int red, int green, int yellow, int blue)
{
    digitalWrite(redPin, red);
    digitalWrite(greenPin, green);
    digitalWrite(yellowPin, yellow);
    digitalWrite(bluePin, blue);
}

void LEDControl::turnOff()
{
    digitalWrite(redPin, LOW);
    digitalWrite(greenPin, LOW);
    digitalWrite(yellowPin, LOW);
    digitalWrite(bluePin, LOW);
}
