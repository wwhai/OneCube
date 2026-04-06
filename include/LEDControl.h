#ifndef LEDCONTROL_H
#define LEDCONTROL_H

#include <Arduino.h>

// LED bit masks – use in protocol CMD_SET_LED / CMD_SET_BLINK
#define LED_RED_MASK    0x01u
#define LED_GREEN_MASK  0x02u
#define LED_YELLOW_MASK 0x04u
#define LED_BLUE_MASK   0x08u
#define LED_ALL_MASK    0x0Fu

class LEDControl
{
public:
    LEDControl(int redPin, int greenPin, int yellowPin, int bluePin);

    void    init();

    // Set one or more LEDs on/off immediately (stops any active blink)
    void    setLed(uint8_t mask, bool on);

    // Turn off all LEDs and cancel all blinks
    void    turnOff();

    // Start non-blocking blink; count=0 means infinite
    void    setBlink(uint8_t mask, uint16_t onMs, uint16_t offMs, uint8_t count);

    // Stop blink and turn off the selected LEDs
    void    stopBlink(uint8_t mask);

    // Returns current on/off bitmask
    uint8_t getState() const;

    // Must be called every loop() iteration
    void    update();

private:
    int     pins[4];     // index: 0=R 1=G 2=Y 3=B
    uint8_t ledState;    // bitmask of logical on/off

    struct BlinkCtrl
    {
        uint16_t      onMs;
        uint16_t      offMs;
        uint8_t       count;      // 0 = infinite
        uint8_t       remaining;
        bool          phase;      // true = ON phase
        bool          active;
        unsigned long lastMs;
    } blinks[4];
};

#endif // LEDCONTROL_H
