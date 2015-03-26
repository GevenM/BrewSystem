// 
// 
// 

#include "HeatingElement.h"


HeatingElement::HeatingElement( uint8_t pin, bool power )
	: outputPin( pin ) ,
	  outputStatus( false )
{
	pinMode(pin, OUTPUT);
	
	if ( power ){
		Activate();
	}
}

void HeatingElement::Activate(){
	outputStatus = true;
	digitalWrite( outputPin, HIGH);
}

void HeatingElement::Deactivate(){
	outputStatus = false;
	digitalWrite( outputPin, LOW);
}

bool HeatingElement::IsActive(){
	return outputStatus;
}