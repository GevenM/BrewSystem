
#include "TempSensor.h"
#include <string.h>

int TempSensor::numberOfSensors = 0;

int TempSensor::GetNumberOfSensors(){
	return numberOfSensors;
}


TempSensor::TempSensor( char *inName, float inTemp, uint8_t *inAddr ){
	name = new char[ strlen( inName ) + 1 ];
	strcpy( name, inName );
	
	SetTemp( inTemp );
	SetAddress( inAddr );
	
	numberOfSensors++;
}

void TempSensor::SetTemp( float inTemp ){
	temperature = inTemp;
}

void TempSensor::SetAddress( uint8_t *addr ){
	for( int i = 0; i<8; i++ ){
		address[i]=addr[i];
	}
}

void TempSensor::SetName( char * inName ){
	delete [] name;
	name = new char[ strlen( inName ) + 1 ];
	strcpy( name, inName );
}


float TempSensor::GetTemp(){
	return temperature;
}

char *TempSensor::GetName(){
	return name;
}

void TempSensor::GetAddress( uint8_t * addr){
	for( int i = 0; i<8; i++ ){
		addr[i]=address[i];
	}
}

TempSensor::~TempSensor(){
	delete [] name;
	numberOfSensors--;
}

void TempSensor::SetPresence( bool present ){
	isPresent = present;
}

bool TempSensor::IsPresent(){
	return isPresent;	
}