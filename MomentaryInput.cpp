// 
// 
// 

#include "MomentaryInput.h"

MomentaryInput::MomentaryInput(uint8_t pin)
: inputPin( pin )
{
	pinMode( pin, INPUT_PULLUP );
}

bool MomentaryInput::IsPressed(){
	return !digitalRead( inputPin );
}

