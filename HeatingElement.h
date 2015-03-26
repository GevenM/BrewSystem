// HeatingElement.h

#ifndef _HEATINGELEMENT_h
#define _HEATINGELEMENT_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif


class HeatingElement{
public:
	HeatingElement( uint8_t pin, uint8_t line );
	
	void Activate();
	void Deactivate();
	bool IsActive();
	
private:	
	const uint8_t outputPin;
	const uint8_t assignedPowerLine;
	bool outputStatus; 
	
	static bool powerLineStatus[3];
};


#endif

