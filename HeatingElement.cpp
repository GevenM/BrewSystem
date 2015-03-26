// 
// 
// 

#include "HeatingElement.h"

bool HeatingElement::powerLineStatus[3] = {false, false, false};


// Heating Element Constructor
// pin specifies the output pin on the arduino.
// line specifies which power line the heating element is on. Range is 0-2. Need to make sure that no more than one heating element per power line can be active. 
// power specifies whether to turn the device on or not.
HeatingElement::HeatingElement( uint8_t pin, uint8_t line )
	: outputPin( pin ) ,
	  assignedPowerLine( line ),
	  outputStatus( false )
{
	pinMode(pin, OUTPUT);
}

// Activates the pin that the heating element is on only if there is no other heating element on that power line on. 
void HeatingElement::Activate(){
	if ( powerLineStatus[ assignedPowerLine ] == false){
		outputStatus = true;
		powerLineStatus[ assignedPowerLine ] = true;
		digitalWrite( outputPin, HIGH);
	}
}

// Deactivates the pin that the heating element is on.
void HeatingElement::Deactivate(){
	outputStatus = false;
	powerLineStatus[ assignedPowerLine ] = false;
	digitalWrite( outputPin, LOW);
}

// Returns true if the heating element is on.
bool HeatingElement::IsActive(){
	return outputStatus;
}