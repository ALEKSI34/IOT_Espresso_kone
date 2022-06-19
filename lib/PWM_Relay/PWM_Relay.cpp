

#include "PWM_Relay.h"
#include "Arduino.h"


class PWM_Relay
{
private:
    /* data */
    unsigned int _frequency;
    unsigned int _RelayPin;
    bool _enabled;
public:

    PWM_Relay( unsigned int RelayPin, unsigned int frequency);
    ~PWM_Relay();

    void EnableOutput();
    void DisableOutput();
    void Execute();

    float DutyCycle;
};

PWM_Relay::PWM_Relay (
 unsigned int RelayPin, unsigned int frequency
) :
    _RelayPin(RelayPin),
    _frequency(frequency)
{
}
void PWM_Relay::Execute() 
{
    float StepSize = 1000.0/float(_frequency); // Step size in MS
    if (!_enabled) {
        digitalWrite(_RelayPin, LOW);
        vTaskDelay(StepSize / portTICK_PERIOD_MS);
    }
    int t_closed = StepSize*DutyCycle;
    int t_open = StepSize*(1-DutyCycle);
    digitalWrite(_RelayPin, HIGH);
    vTaskDelay(t_closed / portTICK_PERIOD_MS);
    digitalWrite(_RelayPin, LOW);
    vTaskDelay(t_open / portTICK_PERIOD_MS);
}
void PWM_Relay::EnableOutput() {
    _enabled = true;
}
void PWM_Relay::DisableOutput(){
    _enabled = false;
}

PWM_Relay::~PWM_Relay()
{   
    
}
