

#ifndef _PID_H_
#define _PID_H_

class PI_Controller
{
private:
    /* data */
    float _Kp;
    float _Ki;
    float _DT;
    float _integral;
    float _error;
    float _inmin;
    float _inmax;
    float _outmin;
    float _outmax;
    float _antiwindup;
    bool _enabled;
public:
    float reference;
    float output;

    float Calculate(float feedback);

    PI_Controller(float Kp, float Ki, float OutputRange[2], float antiwindup, unsigned int stepsize);
    ~PI_Controller();
};


#endif