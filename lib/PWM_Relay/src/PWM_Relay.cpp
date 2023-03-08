

#include "PWM_Relay.h"
#include "Arduino.h"

PWM_Relay::PWM_Relay (
 unsigned int RelayPin, unsigned int frequency
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

    static float PWM_PERIOD = PWM_RESOLUTION/double(this->_basefrequency); // Period of one PWM. in ms

    for (int i = 0; i < PWM_PERIOD; i++) {
        if (i < (DutyCycle * PWM_PERIOD / PWM_RESOLUTION)) {
            digitalWrite(this->_RelayPin, HIGH);
        } else {
            digitalWrite(this->_RelayPin, LOW);
        }
    }
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
