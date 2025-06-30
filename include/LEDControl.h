#ifndef LEDCONTROL_H
#define LEDCONTROL_H

class LEDControl
{
public:
    LEDControl(int redPin, int greenPin, int yellowPin, int bluePin);
    void init();
    void setColor(int red, int green, int yellow, int blue);
    void turnOff();

private:
    int redPin, greenPin, yellowPin, bluePin;
};

#endif
