

#include "PWM_Relay.h"
#include "Arduino.h"

PWM_Relay::PWM_Relay (
 unsigned int RelayPin, unsigned int frequency, unsigned int _stepsize
) :
    _RelayPin(RelayPin),
    _basefrequency(frequency),
    _stepsize(_stepsize)
{
}
void PWM_Relay::Execute() 
{
    // Enable for relay
    if (!this->_enabled) {
        digitalWrite(this->_RelayPin, LOW);
        return;
    }

    static float pwmPeriod = 1000.0/float(this->_basefrequency); // Period of one PWM. in ms
    static int stepsToPeriod = int(pwmPeriod)/this->_stepsize;
    static int loop = 0;
    
    if (loop >= stepsToPeriod){
        loop = 0;
    }

    if (loop < (stepsToPeriod*DutyCycle)){
        digitalWrite(this->_RelayPin, HIGH);
    } else {
        digitalWrite(this->_RelayPin, LOW);
    }

    loop += 1;
}
void PWM_Relay::EnableOutput() {
    this->_enabled = true;
}
void PWM_Relay::DisableOutput(){
    this->_enabled = false;
}

PWM_Relay::~PWM_Relay()
{   
    
}
