#ifndef BEEPCONTROL_H
#define BEEPCONTROL_H

class BeepControl
{
public:
    BeepControl(int beepPin);
    void init();
    void beep(int frequency, int duration);
    void stopBeep();

private:
    int beepPin;
};

#endif
