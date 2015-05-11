// 
// 
// 

#include "MomentaryInput.h"

#define LongPressThreshold 2000
long initialPressTime = -1;
long lastPressTime = millis();

MomentaryInput::MomentaryInput(uint8_t pin)
: inputPin( pin )
{
	pinMode( pin, INPUT_PULLUP );
}

bool MomentaryInput::IsPressed(){
	return !digitalRead( inputPin );
}

bool MomentaryInput::LongPressed(){
	if( IsPressed() ){
		if( initialPressTime == -1 ){
			initialPressTime = millis();
		}
	} else {
		if ( initialPressTime != -1 ) {
			lastPressTime = millis();
			if( abs(lastPressTime - initialPressTime) >= LongPressThreshold ){
				initialPressTime = -1;
				return true;
			}
		}
	}
	return false;
}

bool MomentaryInput::ShortPressed(){
	if( IsPressed() ){
		if( initialPressTime == -1 ){
			initialPressTime = millis();
		}
	} else {
		if ( initialPressTime != -1 ) {
			lastPressTime = millis();
			if( abs(lastPressTime - initialPressTime) <= LongPressThreshold ){
				initialPressTime = -1;
				return true;
			}
		}
	}
	return false;
}

long MomentaryInput::LastPressTime(){
	return lastPressTime;
}