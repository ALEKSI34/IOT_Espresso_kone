

#include "PI.h"

PI_Controller::PI_Controller (
float Kp, float Ki, float OutputRange[2], float antiwindup, unsigned int stepsize
) :
    _Kp(Kp),
    _Ki(Ki),
    _DT(stepsize),
    _outmin(OutputRange[0]),
    _outmax(OutputRange[1]),
    _antiwindup(antiwindup),
    _enabled(true),
    _integral(0),
    _error(0)
{
}
float PI_Controller::Calculate(float input) 
{
    if (!_enabled) return 0;

    float error = input - reference;
    float P = _Kp * error;

    _integral += error * float(_DT);

    // Anti-windup
    if (_integral > _antiwindup) {
        _integral = _antiwindup;
    } else if (_integral < -_antiwindup)
    {
        _integral = -_antiwindup;
    }

    float _output = P + _Ki*_integral;


    if (_output > _outmax) {
        _output = _outmax;
    } 
    else if (_output < _outmin) {
        _output = _outmin;
    }


    // Convert value from input range to output range. ie. Celsius to Duty Cycle, or Bar to Pump Range.    
    output = _output;

    _error = error;
    
    return output;
}
PI_Controller::~PI_Controller()
{   
    
}
