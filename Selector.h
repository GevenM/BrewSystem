// Selector.h

#ifndef _SELECTOR_h
#define _SELECTOR_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

class Selector
{
 public:
	Selector( uint8_t pin );
	
	bool IsOn();
	bool IsOff();
	
 private:
	uint8_t inputPin;
};

#endif

