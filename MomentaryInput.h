// MomentaryInput.h

#ifndef _MOMENTARYINPUT_h
#define _MOMENTARYINPUT_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

class MomentaryInput{
	public:
	MomentaryInput( uint8_t pin );
	
	bool IsPressed();
	
	private:
	const uint8_t inputPin;
};


#endif

