#include "LCDControl.h"
#include <LiquidCrystal_I2C.h>
#include <Arduino.h>
LCDControl::LCDControl(int address) : lcd(address, 16, 2)
{
    startMillis = millis(); // Capture the start time (when the program starts)
}

void LCDControl::init()
{
    lcd.begin(16, 2); // Initialize the LCD with 16 columns and 2 rows
    lcd.setBacklight(1);
    lcd.setContrast(100); // Set contrast to a default value (0-100)
    lcd.clear();          // Clear the display
}

void LCDControl::displayMessage(const char *message)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(message);
}

void LCDControl::displayCpuAndMemoryInfo(float cpuUsage, float memoryUsage)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CPU: ");
    lcd.print(cpuUsage);
    lcd.print("%");
    lcd.setCursor(0, 1);
    lcd.print("MEM: ");
    lcd.print(memoryUsage);
    lcd.print("%");
}

void LCDControl::displayTemperatureAndHumidity(float temp, float humidity)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T: ");
    lcd.print(temp);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("H: ");
    lcd.print(humidity);
    lcd.print("%");
}

void LCDControl::displayIpAddress(const char *ipAddress)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IP: ");
    lcd.print(ipAddress);
}

void LCDControl::clearDisplay()
{
    lcd.clear();
}

void LCDControl::displayNewMessage()
{
    lcd.clear();
    lcd.setCursor(0, 0); // First line
    lcd.print("New Message!");
    delay(2000); // Display for 2 seconds
    lcd.clear();
}
void LCDControl::displayTime()
{
    updateTimeDisplay(); // Calculate time difference and update display

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Date: ");
    lcd.print(dateString); // Display date on the first line
    lcd.setCursor(0, 1);
    lcd.print("Time: ");
    lcd.print(timeString); // Display time on the second line
}

// Function to update the time display based on elapsed time since 2025-07-01
void LCDControl::updateTimeDisplay()
{
    unsigned long currentMillis = millis();
    unsigned long elapsedMillis = currentMillis - startMillis;

    // Milliseconds since 2025-07-01 (starting from startMillis)
    unsigned long elapsedSeconds = elapsedMillis / 1000; // Convert to seconds

    unsigned long days = elapsedSeconds / 86400;           // Calculate days
    unsigned long hours = (elapsedSeconds % 86400) / 3600; // Calculate hours
    unsigned long minutes = (elapsedSeconds % 3600) / 60;  // Calculate minutes
    unsigned long seconds = elapsedSeconds % 60;           // Calculate seconds

    // Convert days to date
    unsigned long years = days / 365; // Approximate number of years
    days = days % 365;                // Remainder days after converting to years
    unsigned long months = days / 30; // Approximate number of months
    days = days % 30;                 // Remaining days after converting to months

    // Format date string in YYYY-MM-DD format
    snprintf(dateString, sizeof(dateString), "%04lu-%02lu-%02lu",
             2025 + years, months + 1, days + 1);

    // Format time string in HH:MM:SS format
    snprintf(timeString, sizeof(timeString), "%02lu:%02lu:%02lu", hours, minutes, seconds);
}