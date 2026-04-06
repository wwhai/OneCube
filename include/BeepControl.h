#ifndef BEEPCONTROL_H
#define BEEPCONTROL_H

#include <Arduino.h>

class BeepControl
{
public:
    BeepControl(int beepPin);

    void init();

    // Single beep – non-blocking (uses beepPattern internally)
    void beepOnce(uint16_t frequency, uint16_t durationMs);

    // Repeating pattern; count=0 is invalid (use beepOnce or stop())
    void beepPattern(uint16_t frequency, uint16_t onMs, uint16_t offMs, uint8_t count);

    // Stop immediately
    void stop();

    bool isActive() const;

    // Must be called every loop() iteration
    void update();

private:
    int           beepPin;
    uint16_t      freq;
    uint16_t      onMs;
    uint16_t      offMs;
    uint8_t       count;      // 0 = stop sentinel (not infinite)
    uint8_t       remaining;
    bool          phase;      // true = ON phase
    bool          active;
    unsigned long lastMs;
};

#endif // BEEPCONTROL_H
