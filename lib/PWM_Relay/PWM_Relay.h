

#ifndef _PWM_H_
#define _PWM_H_

#include "Arduino.h"

class PWM_Relay
{
private:
    /* data */
public:
    float DutyCycle;

    void EnableOutput();
    void DisableOutput();
    void Execute();

    float DutyCycle;

    PWM_Relay(unsigned int RelayPin, unsigned int frequency);
    ~PWM_Relay();
};


#endif