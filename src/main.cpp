#include <Arduino.h>
#include "LEDControl.h"
#include "BeepControl.h"
#include "LCDControl.h"

// Pin definitions
const int RED_PIN = 2;
const int GREEN_PIN = 3;
const int BLUE_PIN = 4;
const int YELLOW_PIN = 5;
const int BEEP_PIN = 6;

LEDControl ledControl(RED_PIN, GREEN_PIN, YELLOW_PIN, BLUE_PIN);
BeepControl beepControl(BEEP_PIN);
LCDControl lcdControl(0x27); // I2C address of 1602 LCD

void setup()
{
    Serial.begin(9600);
    ledControl.init();
    beepControl.init();
    lcdControl.init();
    lcdControl.displayMessage("OneCube Init");
    ledControl.setColor(1, 0, 0, 0);
    delay(500);
    ledControl.setColor(0, 1, 0, 0);
    delay(500);
    ledControl.setColor(0, 0, 1, 0);
    delay(500);
    ledControl.setColor(0, 0, 0, 1);
    delay(500);
    ledControl.turnOff();
    beepControl.beep(1000, 500);
    delay(500);
    beepControl.stopBeep();
}

void loop()
{
    lcdControl.displayTime();
    delay(1000);
}
