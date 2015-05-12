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
	
	bool LongPressed();
	bool ShortPressed();
	long LastPressTime();
	
	private:
	bool IsPressed();
	long PressDuration();
	const uint8_t inputPin;
	long initialPressTime;
	long lastPressTime;
	bool readPending;
};


#endif