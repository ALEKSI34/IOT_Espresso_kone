

#include "PI.h"


class PI_Controller
{
private:
    /* data */
    float _Kp;
    float _Ki;
    float _integral;
    float _error;
    float _min;
    float _max;
    float _antiwindup;
    bool _enabled;
public:

    PI_Controller(float Kp, float Ki, float outMin, float outMax, float antiwindup);
    ~PI_Controller();

    void EnableOutput();
    void DisableOutput();
    float Reference;
    float Output;
    float Calculate(float input);
};

PI_Controller::PI_Controller (
float Kp, float Ki, float outMin, float outMax, float antiwindup
) :
    _Kp(Kp),
    _Ki(Ki),
    _min(outMin),
    _max(outMax),
    _antiwindup(antiwindup),
    _enabled(true),
    _integral(0),
    _error(0)
{
}
float PI_Controller::Calculate(float input) 
{
    if (!_enabled) return 0;

    float error = input - Reference;
    float P = _Kp * error;

    _integral += error;

    // Anti-windup
    if (_integral > _antiwindup) {
        _integral = _antiwindup;
    } else if (_integral < -_antiwindup)
    {
        _integral = -_antiwindup;
    }

    // The actual output
    float _output = P + _Ki*_integral;
    if (_output > _max) {
        Output = _max;
    } else if (_output < _min)
    {
        Output = _min;
    } else {
        Output = _output;
    }

    _error = error;
    
    return Output;
}
void PI_Controller::EnableOutput() {
    _enabled = true;
}
void PI_Controller::DisableOutput(){
    _enabled = false;
}

PI_Controller::~PI_Controller()
{   
    
}
