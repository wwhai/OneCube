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

void LCDControl::displayClock(uint32_t epochSeconds)
{
    unsigned long days = epochSeconds / 86400UL;
    unsigned long secsOfDay = epochSeconds % 86400UL;

    unsigned long hour = secsOfDay / 3600UL;
    unsigned long minute = (secsOfDay % 3600UL) / 60UL;
    unsigned long second = secsOfDay % 60UL;

    // Convert days since 1970-01-01 to Y/M/D using civil algorithm
    long z = (long)days;
    z += 719468L;
    long era = (z >= 0 ? z : z - 146096L) / 146097L;
    unsigned long doe = (unsigned long)(z - era * 146097L);
    unsigned long yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    long y = (long)yoe + era * 400;
    unsigned long doy = doe - (365*yoe + yoe/4 - yoe/100);
    unsigned long mp = (5*doy + 2) / 153;
    unsigned long d = doy - (153*mp + 2) / 5 + 1;
    unsigned long m = mp + (mp < 10 ? 3 : -9);
    y += (m <= 2);

    char line1[17];
    snprintf(line1, sizeof(line1), "%04ld-%02lu-%02lu", y, m, d);
    char line2[17];
    snprintf(line2, sizeof(line2), "%02lu:%02lu:%02lu", hour, minute, second);

    displayLine(0, line1);
    displayLine(1, line2);
}

