// 
// 
// 

#include "SimpleActuator.h"

// Actuator Constructor
// pin specifies the output pin on the arduino.
SimpleActuator::SimpleActuator( uint8_t pin, bool normallyClosed )
: outputPin( pin ) , normallyClosed( normallyClosed ),
outputStatus( false )
{
	pinMode(pin, OUTPUT);
	Deactivate();
}

// Activates the pin that the actuator is on.
void SimpleActuator::Activate(){
	outputStatus = true;
	if( normallyClosed ){
		digitalWrite( outputPin, LOW);
	} else {
		digitalWrite( outputPin, HIGH);
	}
}

// Deactivates the pin that the actuator is on.
void SimpleActuator::Deactivate(){
	outputStatus = false;
	if( normallyClosed ){
		digitalWrite( outputPin, HIGH);
	} else {
		digitalWrite( outputPin, LOW);
	}
}

// Returns true if the heating element is on.
bool SimpleActuator::IsActive(){
	return outputStatus;
}
