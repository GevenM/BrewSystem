// 
// 
// 

#include "SimpleActuator.h"

// Actuator Constructor
// pin specifies the output pin on the arduino.
SimpleActuator::SimpleActuator( uint8_t pin )
: outputPin( pin ) ,
outputStatus( false )
{
	pinMode(pin, OUTPUT);
}

// Activates the pin that the actuator is on.
void SimpleActuator::Activate(){
	outputStatus = true;
	digitalWrite( outputPin, HIGH);
}

// Deactivates the pin that the actuator is on.
void SimpleActuator::Deactivate(){
	outputStatus = false;
	digitalWrite( outputPin, LOW);
}

// Returns true if the heating element is on.
bool SimpleActuator::IsActive(){
	return outputStatus;
}
