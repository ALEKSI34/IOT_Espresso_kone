#include "Arduino.h"
#include "RTOS_PSM.h"

RTOS_PSM *_thePSM;

SemaphoreHandle_t PSMInterruptSemaphore = xSemaphoreCreateBinary();

RTOS_PSM::RTOS_PSM(unsigned char sensePin, unsigned char controlPin, unsigned int range, int mode, unsigned char divider, unsigned char interruptMinTimeDiff)
{
	_thePSM = this;

	pinMode(sensePin, INPUT_PULLUP);
	RTOS_PSM::_sensePin = sensePin;

	pinMode(controlPin, OUTPUT);
	RTOS_PSM::_controlPin = controlPin;
	
	RTOS_PSM::_divider = divider > 0 ? divider : 1;

	uint8_t interruptNum = digitalPinToInterrupt(RTOS_PSM::_sensePin);

	if (interruptNum != NOT_AN_INTERRUPT)
	{
		attachInterrupt(interruptNum, RTOS_PSM::onInterrupt, mode);
	}
	RTOS_PSM::_range = range;

	RTOS_PSM::_interruptMinTimeDiff = interruptMinTimeDiff;

    xTaskCreatePinnedToCore(
        InterruptTask,
        "PSM_Modulation_Task",
        10000,
        this,
        1,
        &xInterruptTaskHandle,
        !CONFIG_ARDUINO_RUNNING_CORE
    );
}
void RTOS_PSM::onInterrupt()
{
    xSemaphoreGiveFromISR(PSMInterruptSemaphore, NULL);
}
void RTOS_PSM::InterruptTask(void * parameters)
{
    RTOS_PSM* pRTOS_PSM = (RTOS_PSM*)parameters;
    for (;;)
    {
        if (xSemaphoreTake(PSMInterruptSemaphore, portMAX_DELAY) == pdTRUE)
        {
            pRTOS_PSM->InterruptHandler();
        }
    }
	vTaskDelete(NULL);
}
void RTOS_PSM::InterruptHandler()
{
	if (_thePSM->_interruptMinTimeDiff > 0 && millis() - _thePSM->_lastMillis < _thePSM->_interruptMinTimeDiff) {
		return;
	}
	_thePSM->_lastMillis = millis();

	if (_thePSM->_dividerCounter >= _thePSM->_divider)
	{
		_thePSM->_dividerCounter = 1;
	
		_thePSM->calculateSkip();
	}
	else
	{
		_thePSM->_dividerCounter++;
	}
}

void RTOS_PSM::set(unsigned int value)
{
	if (value < RTOS_PSM::_range)
	{
		RTOS_PSM::_value = value;
	}
	else
	{
		RTOS_PSM::_value = RTOS_PSM::_range;
	}
}

long RTOS_PSM::getCounter()
{
	return RTOS_PSM::_counter;
}

void RTOS_PSM::resetCounter()
{
	RTOS_PSM::_counter = 0;
}

void RTOS_PSM::stopAfter(long counter)
{
	RTOS_PSM::_stopAfter = counter;
}

void RTOS_PSM::calculateSkip()
{
	RTOS_PSM::_tempvalue += RTOS_PSM::_value;

	if (RTOS_PSM::_tempvalue >= RTOS_PSM::_range)
	{
		RTOS_PSM::_tempvalue -= RTOS_PSM::_range;
		RTOS_PSM::_skip = false;
	}
	else
	{
		RTOS_PSM::_skip = true;
	}

	if (RTOS_PSM::_tempvalue > RTOS_PSM::_range)
	{
		RTOS_PSM::_tempvalue = 0;
		RTOS_PSM::_skip = false;
	}

	if (!RTOS_PSM::_skip)
	{
		RTOS_PSM::_counter++;
	}

	if (!RTOS_PSM::_skip 
		&& RTOS_PSM::_stopAfter > 0
		&& RTOS_PSM::_counter > RTOS_PSM::_stopAfter)
	{
		RTOS_PSM::_skip = true;
	}

	updateControl();
}

void RTOS_PSM::updateControl()
{
	if (RTOS_PSM::_skip)
	{
		digitalWrite(RTOS_PSM::_controlPin, LOW);
	}
	else
	{
		digitalWrite(RTOS_PSM::_controlPin, HIGH);
	}
}

unsigned int RTOS_PSM::cps()
{
    unsigned int range = RTOS_PSM::_range;
    unsigned int value = RTOS_PSM::_value;
    
    RTOS_PSM::_range = 0xFFFF;
    RTOS_PSM::_value = 1;
    RTOS_PSM::_tempvalue = 0;
    
    unsigned long stopAt = millis() + 1000;  
    
    while (millis() < stopAt)
    {
        delay(0);
    }
    
    unsigned int result = RTOS_PSM::_tempvalue;
    
    RTOS_PSM::_range = range;
    RTOS_PSM::_value = value;
    RTOS_PSM::_tempvalue = 0;
    
    return result;
}

unsigned long RTOS_PSM::getLastMillis()
{
	return RTOS_PSM::_lastMillis;
}