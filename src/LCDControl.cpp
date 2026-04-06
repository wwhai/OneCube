#include "LCDControl.h"
#include <Arduino.h>
#include <stdio.h>

LCDControl::LCDControl(uint8_t address)
    : lcd(address, 16, 2), startMillis(0)
{}

void LCDControl::init()
{
    startMillis = millis();
    lcd.init();
    lcd.backlight();
    lcd.clear();
}

void LCDControl::displayText(uint8_t row, uint8_t col, const char *text)
{
    if (row > 1 || col > 15)
        return;
    lcd.setCursor(col, row);
    lcd.print(text);
}

void LCDControl::displayLine(uint8_t row, const char *text)
{
    if (row > 1)
        return;
    char buf[17];
    snprintf(buf, sizeof(buf), "%-16s", text);  // left-align, pad to 16 chars
    lcd.setCursor(0, row);
    lcd.print(buf);
}

void LCDControl::clearDisplay()
{
    lcd.clear();
}

void LCDControl::displayUptime()
{
    unsigned long elapsed = (millis() - startMillis) / 1000UL;
    unsigned long d = elapsed / 86400UL; elapsed %= 86400UL;
    unsigned long h = elapsed / 3600UL;  elapsed %= 3600UL;
    unsigned long m = elapsed / 60UL;
    unsigned long s = elapsed % 60UL;

    char line1[17];
    snprintf(line1, sizeof(line1), "Up:%02lud %02lu:%02lu:%02lu", d, h, m, s);
    displayLine(0, "OneCube v1.0");
    displayLine(1, line1);
}

