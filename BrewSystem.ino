#include "TempSensor.h"
#include "Recipe.h"
#include "HeatingElement.h"
#include <string.h>
#include <SPI.h>
#include <SD.h>
#include <Ethernet.h>
#include <OneWire.h>
#include <TimerOne.h>


y_recipe theGlobalRecipe;

#define ONE_WIRE_PIN 11

OneWire  ds( ONE_WIRE_PIN );  // (a 4.7K resistor is necessary)

TempSensor tempSensor[3];

HeatingElement boilElement1(23, 0);
HeatingElement boilElement2(25, 1);
HeatingElement boilElement3(27, 2);
HeatingElement mashElement(29, 2);
HeatingElement hltElement1(31, 0);
HeatingElement hltElement2(33, 1);
HeatingElement hltElement3(35, 2);

uint8_t addr1[8] = {0x28, 0xA3, 0xEF, 0x5C, 0x6, 0x0, 0x0, 0xB5};
uint8_t addr2[8] = {0x28, 0xED, 0x3A, 0x5D, 0x6, 0x0, 0x0, 0xD0};
uint8_t addr3[8] = {0x28, 0x93, 0x31, 0x5D, 0x6, 0x0, 0x0, 0x2B};
	


// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0F, 0x4D, 0x2A };
IPAddress ip(192,168,0, 112);
byte gateway[] = { 192, 168, 0, 1 };
byte subnet[] = { 255, 255, 255, 0 };

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

void ISR_TempTimer();
void StartTempConversion();
void ReadTemperatureSensors();
void SetTempResolution( OneWire myds );
void UpdateTempSensor( TempSensor * sensor );
	
void setup() {

	tempSensor[0].SetAddress(addr1);
	tempSensor[0].SetName("Boil");
	tempSensor[1].SetAddress(addr2);
	tempSensor[1].SetName("Mash");
	tempSensor[2].SetAddress(addr3);
	tempSensor[2].SetName("HLT");
		
	Serial.begin(9600);
	
	Timer1.initialize(1000000); // initialized at 1 sec
	Timer1.attachInterrupt( ISR_TempTimer );

	// disable w5100 while setting up SD
	pinMode(10,OUTPUT);
	digitalWrite(10,HIGH);

	if(SD.begin(4) == 0) Serial.println("SD fail");
	else Serial.println("SD ok");

	Ethernet.begin(mac,ip);
	digitalWrite(10,HIGH);

	Serial.print("server is at ");
	Serial.println(Ethernet.localIP());
	
	SetTempResolution( ds );
	StartTempConversion();
}




void loop()
{
	// listen for incoming clients
	EthernetClient client = server.available();
	if (client) {
		Serial.println("new client");
		// an http request ends with a blank line
		boolean currentLineIsBlank = true;
		while (client.connected()) {
			if (client.available()) {
				char c = client.read();
				Serial.write(c);
				// if you've gotten to the end of the line (received a newline
				// character) and the line is blank, the http request has ended,
				// so you can send a reply
				if (c == '\n' && currentLineIsBlank) {
					// send a standard http response header
					client.println("HTTP/1.1 200 OK");
					client.println("Content-Type: text/html");
					client.println("Connection: close");  // the connection will be closed after completion of the response
					client.println("Refresh: 5");  // refresh the page automatically every 5 sec
					client.println();
					client.println("<!DOCTYPE HTML>");
					client.println("<html>");
					client.println("NEW LIMBURG BREWERY <br>");
				
					client.println("Brew System");
					client.println("<br><br>");
					client.println("Temperatures:<br>");
					for ( int sensor = 0 ; sensor < TempSensor::GetNumberOfSensors() ; sensor++ ){
						client.println( tempSensor[sensor].GetName() );
						client.println(": ");
						if ( tempSensor[sensor].IsPresent()){
							client.println( tempSensor[sensor].GetTemp() );
							client.println( " Celcius. <br>");
						} else {
							client.println( "not detected.<br>");
						}
					
					}

					client.println("</html>");
					break;
				}
				if (c == '\n') {
					// you're starting a new line
					currentLineIsBlank = true;
				}
				else if (c != '\r') {
					// you've gotten a character on the current line
					currentLineIsBlank = false;
				}
			}
		}
		// give the web browser time to receive the data
		delay(1);
		// close the connection:
		client.stop();
		Serial.println("client disconnected");
	}

}

void SetTempResolution( OneWire myds ) {
	myds.reset();
	myds.skip(); // skip rom
	myds.write(0x3F);
}

void ISR_TempTimer( ){
	ReadTemperatureSensors();
	StartTempConversion();
}

void StartTempConversion(){
	ds.skip(); // skip rom
	ds.write(0x44); // sends a temp conversion command to all the sensors
}

void populateRecipe(){
	theGlobalRecipe.BoilSize = 200;
	strncpy( theGlobalRecipe.Fermentables[1].Name, "pilsner malt", k_nameLength-1);
	theGlobalRecipe.Fermentables[1].Amount = 20;
	theGlobalRecipe.Hops[1].Amount = 20;
	strncpy( theGlobalRecipe.Hops[1].Name, "Czech saaz", k_nameLength-1);
	theGlobalRecipe.Hops[1].Time = 60;
	theGlobalRecipe.Hops[1].Use = e_use_boil;
	
	theGlobalRecipe.Mash.Step[1].InfuseAmount = 10;
	

}


bool TempSensorPresent( TempSensor * sensor ){
	byte i;
	bool present = false;
	
	byte addFound[8];
	byte addKnown[8];
	
	ds.reset_search();
	sensor->GetAddress(addKnown);

	while( ds.search(addFound) ){
		if (OneWire::crc8(addFound, 7) != addFound[7]) {
			Serial.println("CRC is not valid!");
			return false;
		}
		for( int j = 0 ; j < 8; j++ ){
			if (addFound[j] != addKnown[j]){
				break;
			}
			if ( j == 7 ){
				present = true;
				break;
			}
		}
	}
	return present;
}



void ReadTemperatureSensors(){
	byte i;
	byte present = 0;
	byte type_s;
	byte data[12];
	byte addFound[8];
	byte addKnown[8];
	float celsius, fahrenheit;
	bool found = false;
	
	Serial.println(" ");
	Serial.print("Number of Sensors: ");
	Serial.println(TempSensor::GetNumberOfSensors());
	
	for( i = 0; i <  TempSensor::GetNumberOfSensors(); i++ ){
		
		tempSensor[i].SetPresence( TempSensorPresent( &tempSensor[i]));
		
		if ( tempSensor[i].IsPresent() ){
			
			UpdateTempSensor( &tempSensor[i] );
			Serial.print( tempSensor[i].GetName() );
			Serial.print(" temperature ");
			Serial.print( tempSensor[i].GetTemp() );
			Serial.println(" Celcius.");

			} else {
			Serial.print( tempSensor[i].GetName() );
			Serial.println(" NOT found" );
		}
	}
	return;
}


void UpdateTempSensor( TempSensor * sensor ){
	byte i;
	byte data[12];
	byte addr[8];
	byte type_s;
	float celsius, fahrenheit;
	
	ds.reset();
	sensor->GetAddress(addr);
	ds.select( addr );
	ds.write( 0xBE );         // Read Scratchpad
	

	for ( i = 0; i < 9; i++) {           // we need 9 bytes
		data[i] = ds.read();
	}

	// Convert the data to actual temperature
	// because the result is a 16 bit signed integer, it should
	// be stored to an "int16_t" type, which is always 16 bits
	// even when compiled on a 32 bit processor.
	int16_t raw = (data[1] << 8) | data[0];
	if (type_s) {
		raw = raw << 3; // 9 bit resolution default
		if (data[7] == 0x10) {
			// "count remain" gives full 12 bit resolution
			raw = (raw & 0xFFF0) + 12 - data[6];
		}
		} else {
		byte cfg = (data[4] & 0x60);
		// at lower res, the low bits are undefined, so let's zero them
		if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
		else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
		else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
		//// default is 12 bit resolution, 750 ms conversion time
	}
	
	celsius = (float)raw / 16.0;
	sensor->SetTemp( celsius );
	ds.reset();

}