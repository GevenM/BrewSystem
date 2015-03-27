// 
// 
// 

#include "Selector.h"


Selector::Selector(uint8_t pin)
	: inputPin( pin )
{
	pinMode( pin, INPUT_PULLUP );
}

bool Selector::IsOn(){
	return !digitalRead( inputPin );
}

bool Selector::IsOff(){
	return !IsOn();
}

