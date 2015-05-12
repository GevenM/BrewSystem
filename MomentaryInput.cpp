// 
// 
// 

#include "MomentaryInput.h"

#define LongPressThreshold 2000
long initialPressTime = -1;
long lastPressTime = millis();
bool readPending = true;

MomentaryInput::MomentaryInput(uint8_t pin)
: inputPin( pin )
{
	pinMode( pin, INPUT_PULLUP );
}

bool MomentaryInput::IsPressed(){
	return !digitalRead( inputPin );
}

long MomentaryInput::PressDuration(){
	return abs( millis() - initialPressTime );
}

bool MomentaryInput::LongPressed(){
	if( IsPressed() ){
		if( readPending == false ){
			initialPressTime = millis();
			readPending = true;
			
		} else if( PressDuration() >= LongPressThreshold ){
			lastPressTime = millis();
			return true;
		}
	} else {
		if ( readPending == true ) {
			if( PressDuration() >= LongPressThreshold ){
				lastPressTime = millis();
				readPending = false;
				return true;
			}
		}
	}
	return false;
}

bool MomentaryInput::ShortPressed(){
	if( IsPressed() ){
		if( readPending == false ){
			initialPressTime = millis();
			readPending = true;
		}
		
	} else {
		if ( readPending == true ) {
			if( PressDuration() < LongPressThreshold ){
				initialPressTime = -1;
				lastPressTime = millis();
				return true;
			}
		}
	}
	return false;
}

long MomentaryInput::LastPressTime(){
	return lastPressTime;
}