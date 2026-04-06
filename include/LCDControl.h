#ifndef LCDCONTROL_H
#define LCDCONTROL_H

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

class LCDControl
{
public:
    explicit LCDControl(uint8_t address);

    void init();

    // Place text at an arbitrary cursor position (does NOT clear the line)
    void displayText(uint8_t row, uint8_t col, const char *text);

    // Overwrite an entire row with text, padding with spaces
    void displayLine(uint8_t row, const char *text);

    void clearDisplay();

    // Standalone uptime display (shown when host is idle)
    void displayUptime();

private:
    LiquidCrystal_I2C lcd;
    unsigned long     startMillis;
};

#endif // LCDCONTROL_H
