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
	HeatingElement( uint8_t , bool = false );
	
	void Activate();
	void Deactivate();
	bool IsActive();
	
private:	
	const uint8_t outputPin;
	bool outputStatus; 
	
};


#endif

