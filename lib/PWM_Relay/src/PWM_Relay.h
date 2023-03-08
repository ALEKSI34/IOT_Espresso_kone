

#ifndef _PWM_H_
#define _PWM_H_

#define PWM_RESOLUTION 1000

#include "Arduino.h"

class PWM_Relay
{
private:
    /* data */
    unsigned int _basefrequency;
    unsigned int _stepsize;
    unsigned int _RelayPin;
    bool _enabled;
public:
    float DutyCycle;

    void EnableOutput();
    void DisableOutput();
    void Execute();

    PWM_Relay( unsigned int RelayPin, unsigned int frequency);
    ~PWM_Relay();
};

#endif