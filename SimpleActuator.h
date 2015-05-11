// SimpleActuator.h

#ifndef _SIMPLEACTUATOR_h
#define _SIMPLEACTUATOR_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

class SimpleActuator{
	public:
	SimpleActuator( uint8_t pin );
	
	void Activate();
	void Deactivate();
	bool IsActive();
	
	private:
	const uint8_t outputPin;
	bool outputStatus;
};


#endif

