#ifndef LCDCONTROL_H
#define LCDCONTROL_H

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

class LCDControl
{
public:
    LCDControl(int address);
    void init();
    void displayMessage(const char *message);
    void displayCpuAndMemoryInfo(float cpuUsage, float memoryUsage);
    void displayTemperatureAndHumidity(float temp, float humidity);
    void displayIpAddress(const char *ipAddress);
    void clearDisplay();
    void displayNewMessage();
    void displayTime();

private:
    LiquidCrystal_I2C lcd;
    unsigned long startMillis; // Start time (in milliseconds since the program started)
    void updateTimeDisplay();
    char dateString[11]; // Buffer to hold formatted date string (YYYY-MM-DD)
    char timeString[9];  // Buffer to hold formatted time string (HH:MM:SS)
};

#endif
