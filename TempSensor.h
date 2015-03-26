
#ifndef _TEMPSENSOR_h
#define _TEMPSENSOR_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

class TempSensor {
public:
	TempSensor( char * = NULL, float = 0, uint8_t * = NULL );
	~TempSensor();
	
	void SetTemp( float );
	void SetAddress( uint8_t * );
	void SetName( char * );
	
	float GetTemp();
	void GetAddress( uint8_t * addr );
	char *GetName();
	static int GetNumberOfSensors();
	
private:
	float temperature;
	byte address[8];
	char *name;
	bool present;
	
	static int numberOfSensors;
};

#endif