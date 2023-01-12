#ifndef PSM_h
#define PSM_h

#include <freertos/semphr.h>
#include "Arduino.h"

 
class RTOS_PSM
{
public:
	RTOS_PSM(unsigned char sensePin, unsigned char controlPin, unsigned int range, int mode = RISING, unsigned char divider = 1, unsigned char interruptMinTimeDiff = 0);

	void set(unsigned int value);

	long getCounter();
	void resetCounter();

	void stopAfter(long counter);
    
    unsigned int cps();
	unsigned long getLastMillis();

private:
    static inline void InterruptHandler();
	void calculateSkip();
	void updateControl();
	 
	unsigned char _sensePin;
	unsigned char _controlPin;
	unsigned int _range;
	unsigned char _divider;
	unsigned char _dividerCounter = 1;
	unsigned char _interruptMinTimeDiff;
	volatile unsigned int _value;
	volatile unsigned int _tempvalue;
	volatile bool _skip;
	volatile long _counter;
	volatile long _stopAfter;
	volatile unsigned long _lastMillis;


    TaskHandle_t xInterruptTaskHandle;

    static void InterruptTask(void * parameters);
    static inline void onInterrupt(void);
};

extern RTOS_PSM *_thePSM;

#endif