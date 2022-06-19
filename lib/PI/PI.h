

#ifndef _PID_H_
#define _PID_H_

class PI_Controller
{
private:
    /* data */
public:
    float Reference;
    float Output;

    float Calculate(float feedback);

    PI_Controller(float Kp, float Ki, float outMin, float outMax, float antiwindup);
    ~PI_Controller();
};


#endif