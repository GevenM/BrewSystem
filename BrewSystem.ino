#include "MomentaryInput.h"
#include "SimpleActuator.h"
#include "Selector.h"
#include "TempSensor.h"
#include "Recipe.h"
#include "HeatingElement.h"
#include "SimpleActuator.h"


#include <string.h>
#include <SPI.h>
#include <Time.h>
#include <SD.h>
#include <EthernetUdp.h>
#include <Ethernet.h>
#include <OneWire.h>
#include <TimerOne.h>
#include <Wire.h>
#include <Adafruit_LEDBackpack.h>
#include <Adafruit_GFX.h>
#include <PID_v1.h>

y_recipe theGlobalRecipe;


// Global Variables
int m_hltWaterLevel = 0;
#define k_hltMinWaterLevel 50
int m_hltDesiredWaterLevel = 250;
bool screenUpdateFlag = true;

typedef enum {
	e_sysStatus_Standby,
	e_sysStatus_Ready,
	e_sysStatus_Emergency,
} y_sysStatus;
y_sysStatus c_sysStatus = e_sysStatus_Standby;

typedef enum{
	e_hltStatus_Idle,
	e_hltStatus_On,
	e_hltStatus_Empty,
} y_hltStatus;
y_hltStatus c_hltStatus = e_hltStatus_Idle;

typedef enum{
	e_mashStatus_Idle,
	e_mashStatus_StartReq,
	e_mashStatus_MashInReq,
	e_mashStatus_MashIn,
	e_mashStatus_Rest,
	e_mashStatus_MashOut,
	e_mashStatus_PreSparge,
	e_mashStatus_Sparge,
} y_mashStatus;
y_mashStatus c_mashStatus = e_mashStatus_Idle;

bool M_brewStartReq = false;
float f_hltSetpoint = 0;
float recipe_strikeTemp = 76.0;
float hltTempRange = 2;

typedef enum{
	e_menuScreen_Idle,
	e_menuScreen_Temp,
	e_menuScreen_Temp_HLT,
	e_menuScreen_Temp_Mash,
	e_menuScreen_Temp_Boil,
	e_menuScreen_Temp_HLT_Set,
	e_menuScreen_Temp_Mash_Set,
	e_menuScreen_Temp_Boil_Set,
	e_menuScreen_Emergency,
	e_menuScreen_Standby,
	} y_menuScreen;
y_menuScreen c_menuScreen = e_menuScreen_Idle;



/************************* HARDWARE LAYER ***************************/

// One wire bus
#define ONE_WIRE_PIN 11
OneWire  ds( ONE_WIRE_PIN );  // (a 4.7K resistor is necessary)


// led screens (use i2c bus)
Adafruit_AlphaNum4 c_hltDisplay = Adafruit_AlphaNum4();
Adafruit_AlphaNum4 c_mashDisplay = Adafruit_AlphaNum4();
Adafruit_AlphaNum4 c_boilDisplay = Adafruit_AlphaNum4();
Adafruit_AlphaNum4 c_mainQuadDisplay1 = Adafruit_AlphaNum4();
Adafruit_AlphaNum4 c_mainQuadDisplay2 = Adafruit_AlphaNum4();
Adafruit_7segment c_mainDisplay = Adafruit_7segment();


// TEMPERATURE SENSORS
TempSensor tempSensor[3]; // Array holding all the temperature sensors (so that they can be read sequentially using a loop)

// temperature sensors by name so that they can be addressed in a non confusing way.
TempSensor * m_temp_hlt = &tempSensor[0];
TempSensor * m_temp_mash = &tempSensor[1];
TempSensor * m_temp_boil = &tempSensor[2];


// Known addresses of sensors
uint8_t addr1[8] = {0x28, 0x52, 0x01, 0x6F, 0x6, 0x0, 0x0, 0x48};
uint8_t addr2[8] = {0x28, 0xAF, 0x99, 0x6E, 0x6, 0x0, 0x0, 0x76};
uint8_t addr3[8] = {0x28, 0xF0, 0x23, 0x6F, 0x6, 0x0, 0x0, 0x01};


// HEATING ELEMENTS
HeatingElement c_boilElement1(23, 0);
HeatingElement c_boilElement2(25, 1);
HeatingElement c_boilElement3(27, 2);
HeatingElement c_mashElement(29, 2);
HeatingElement c_hltElement1(31, 0);
HeatingElement c_hltElement2(33, 1);
HeatingElement c_hltElement3(35, 2);

// Set up PIDs for heating control
int pidWindowSize = 1000; // 1000 milliseconds 
unsigned long pidWindowStartTime;

double boilPIDSetpoint = 100, boilPIDInput, boilPIDOutput;
PID boilPID( &boilPIDInput, &boilPIDOutput, &boilPIDSetpoint, 40, 0.2, 0.1, DIRECT );

double mashPIDSetpoint, mashPIDInput, mashPIDOutput;
PID mashPID( &mashPIDInput, &mashPIDOutput, &mashPIDSetpoint, 20, 0.1, 1, DIRECT );

double hltPIDSetpoint = 25, hltPIDInput, hltPIDOutput;
PID hltPID( &hltPIDInput, &hltPIDOutput, &hltPIDSetpoint, 20, 0.1, 1, DIRECT );


// PUMPS
SimpleActuator c_wortPump( 22, true );
SimpleActuator c_waterPump( 24, true );
SimpleActuator c_glycolPump( 26, true );
SimpleActuator c_transferPump( 28, true );

// OUTLETS
SimpleActuator c_outlet110( 30, true );
SimpleActuator c_outlet240( 34, true );

// GLYCOL VALVES
SimpleActuator c_glycolValve_FV1( 62, true );
SimpleActuator c_glycolValve_FV2( 63, true );
SimpleActuator c_glycolValve_FV3( 64, true );
SimpleActuator c_glycolValve_FV4( 65, true );
SimpleActuator c_glycolValve_FV5( 66, true );
SimpleActuator c_glycolValve_BB1( 67, true );
SimpleActuator c_glycolValve_chiller( 68, true );


// SPARES
SimpleActuator c_unnassigned1( 32, true );
SimpleActuator c_unnassigned2( 36, true );
SimpleActuator c_glycolValve_unassigned1( 69, true );





// SELECTOR SWITCHES
Selector m_sw_emergency( 14 );
Selector m_sw_power ( 15 );
Selector m_sw_alarm ( 49 );
Selector m_sw_boil( 38 );
Selector m_sw_mash( 40 );
Selector m_sw_hlt( 42 );
Selector m_sw_wortPump( 44 );
Selector m_sw_waterPump( 46 );
Selector m_sw_glycolPump( 48 );
Selector m_sw_transferPump( 39 );
Selector m_sw_outlet110( 41 );
Selector m_sw_outlet240( 43 );

// PUSH BUTTONS
MomentaryInput m_btn_menuLeft( 47 );
MomentaryInput m_btn_menuRight( 45 );

// ETHERNET 
// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0F, 0x4D, 0x2A };
IPAddress ip(192,168,0, 112);
byte gateway[] = { 192, 168, 0, 1 };
byte subnet[] = { 255, 255, 255, 0 };

unsigned int localPort = 8888;      // local port to listen for UDP packets
IPAddress timeServer(66, 113, 32, 5); //ntp server
const int timeZone = -5;

const int NTP_PACKET_SIZE= 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
//const  long timeZoneOffset = -18000L; // set this to the offset in seconds to your local time;

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);
EthernetUDP Udp;

/************************************************************************************/


/************************** FUNTION PROTOTYPES **************************************/
void ISR_TempTimer();
void StartTempConversion();
void ReadTemperatureSensors();
void SetTempResolution( OneWire myds );
void UpdateTempSensor( TempSensor * sensor );
void InitDisplays();
bool UpdateMenu();
void WriteMenu();
void UpdateMonitoredVariables();

void HLTTurnOff();
void HLTControlTemp();
void MashTurnOff();
void MashControlTemp();
void BoilTurnOff();
void BoilControlTemp();

/************************************************************************************/


void setup() {
	Serial.begin(9600);
	delay(250);
	
	// PID setup
	pidWindowStartTime = millis();
	boilPIDSetpoint = 60;
	
	boilPID.SetOutputLimits( 0, pidWindowSize );
	boilPID.SetSampleTime( 5000 );
	boilPID.SetMode( AUTOMATIC );
	mashPID.SetOutputLimits( 0, pidWindowSize );
	mashPID.SetSampleTime( 5000 );
	mashPID.SetMode( AUTOMATIC );
	hltPID.SetOutputLimits( 0, pidWindowSize );
	hltPID.SetSampleTime( 5000 );
	hltPID.SetMode( AUTOMATIC );
	//------
	
	
	// Initialize temperature sensors
	m_temp_hlt->SetAddress( addr1 );
	m_temp_hlt->SetName( "HLT" );
	
	m_temp_mash->SetAddress( addr2 );
	m_temp_mash->SetName( "Mash" );
	
	m_temp_boil->SetAddress( addr3 );
	m_temp_boil->SetName( "Boil" );
	
	
	
	// INITIALIZE LED DISPLAYS
	InitDisplays();

	
	//Serial.begin(9600);
	
	Timer1.initialize(500000); // initialized at .5 sec
	Timer1.attachInterrupt( ISR_TempTimer );

	// disable w5100 while setting up SD
	pinMode(10,OUTPUT);
	digitalWrite(10,HIGH);

//	if(SD.begin(4) == 0) ;//Serial.println("SD fail");
	//else Serial.println("SD ok");

	Ethernet.begin(mac,ip);
	Udp.begin(localPort);
	
	digitalWrite(10,HIGH);
	
	delay (500);
	
	setSyncInterval( 86400 );
	
	Serial.println("waiting for sync");
	int i;
	for( i = 0; i< 5; i++ ){
		Serial.print("try ");
		Serial.print( i );
		Serial.println("");
		
		setSyncProvider(getNtpTime); // wait until the time is set by the sync provider
		if(timeStatus()== timeNotSet){
			Serial.println("sync fail");; // success
			} else {
			Serial.println("sync success");;//failed to set
			break;
		}
		
		delay(5000);
	}
		
	Serial.print("server is at ");
	Serial.println(Ethernet.localIP());
	
	SetTempResolution( ds );
	StartTempConversion();
}


bool AllEquipmentSwitchesOff(){
	if ( m_sw_boil.IsOff() &&
			m_sw_mash.IsOff() &&
			m_sw_hlt.IsOff() &&
			m_sw_wortPump.IsOff() &&
			m_sw_waterPump.IsOff() &&
			//m_sw_glycolPump.IsOff() &&
			//m_sw_transferPump.IsOff() &&
			m_sw_outlet110.IsOff() &&
			m_sw_outlet240.IsOff() ) {
		return true;
	}
	return false;
}

void UpdateSystemStatus(){
	if ( c_sysStatus == e_sysStatus_Standby ){
		if ( m_sw_emergency.IsOn() ){
			if ( m_sw_power.IsOn() ){
				if ( AllEquipmentSwitchesOff() ){
					c_sysStatus = e_sysStatus_Ready;
				} else {
					; //NC
				}
			} else {
				; //NC
			}
		} else {
			c_sysStatus = e_sysStatus_Emergency;
		}
	} else if ( c_sysStatus == e_sysStatus_Ready ){
		if ( m_sw_emergency.IsOn() ){
			if ( m_sw_power.IsOn() ){
				; //NC
			} else {
				c_sysStatus = e_sysStatus_Standby;
			}
		} else {
			c_sysStatus = e_sysStatus_Emergency;
		}
	} else if ( c_sysStatus == e_sysStatus_Emergency ){
		if ( m_sw_emergency.IsOn() ){
			if ( m_sw_power.IsOff() ){
				c_sysStatus = e_sysStatus_Standby;
			} else {
				if ( AllEquipmentSwitchesOff() ){
					c_sysStatus = e_sysStatus_Ready;
				} else {
					c_sysStatus = e_sysStatus_Standby;
				}
			}
		} else {
			c_sysStatus = e_sysStatus_Emergency;
		}
	}
}

void HLTControlTemp(){
	// {CONTROL TEMPERATURE}
		hltPID.SetMode( AUTOMATIC );
		
	// Control output based on the output suggested by pid. Divided into three levels.
	if( hltPIDOutput < pidWindowSize/3 ){
		c_hltElement2.Deactivate();
		c_hltElement3.Deactivate();
		
		if( (hltPIDOutput * 3) > millis() - pidWindowStartTime){
			c_hltElement1.Activate();
		} else {
			c_hltElement1.Deactivate();
		}
		
	} else if ( hltPIDOutput < pidWindowSize/3*2 ){
		c_hltElement1.Activate();
		c_hltElement3.Deactivate();
		
		if( ( (hltPIDOutput - pidWindowSize/3) * 3) > millis() - pidWindowStartTime){
			c_hltElement2.Activate();
		} else {
			c_hltElement2.Deactivate();
		}
		
	} else {
		c_hltElement1.Activate();
		c_hltElement2.Activate();
		
		if( ( (hltPIDOutput - pidWindowSize/3*2) * 3) > millis() - pidWindowStartTime){
			c_hltElement3.Activate();
		} else {
			c_hltElement3.Deactivate();
		}
	}
}

void BoilControlTemp(){
	// {CONTROL TEMPERATURE}
	boilPID.SetMode( AUTOMATIC );
	
	// Control output based on the output suggested by pid. Divided into three levels.
	if( boilPIDOutput < pidWindowSize/3 ){
		c_boilElement2.Deactivate();
		c_boilElement3.Deactivate();
		
		if( (boilPIDOutput * 3) > millis() - pidWindowStartTime){
			c_boilElement1.Activate();
			} else {
			c_boilElement1.Deactivate();
		}
		
		} else if ( boilPIDOutput < pidWindowSize/3*2 ){
		c_boilElement1.Activate();
		c_boilElement3.Deactivate();
		
		if( ( (boilPIDOutput - pidWindowSize/3) * 3) > millis() - pidWindowStartTime){
			c_boilElement2.Activate();
			} else {
			c_boilElement2.Deactivate();
		}
		
		} else {
		c_boilElement1.Activate();
		c_boilElement2.Activate();
		
		if( ( (boilPIDOutput - pidWindowSize/3*2) * 3) > millis() - pidWindowStartTime){
			c_boilElement3.Activate();
			} else {
			c_boilElement3.Deactivate();
		}
	}
}

void MashControlTemp(){
	// {CONTROL TEMPERATURE}
	mashPID.SetMode( AUTOMATIC );
	
	if(  mashPIDOutput > millis() - pidWindowStartTime){
		c_mashElement.Activate();
		} else {
		c_mashElement.Deactivate();
	}
}


void HLTTurnOff(){
	// { TURN OFF ELEMENTS }
	hltPID.SetMode( MANUAL );
	c_hltElement1.Deactivate();
	c_hltElement2.Deactivate();
	c_hltElement3.Deactivate();
	// ---------------------
}

void BoilTurnOff(){
	// { TURN OFF ELEMENTS }
	boilPID.SetMode( MANUAL );
	c_boilElement1.Deactivate();
	c_boilElement2.Deactivate();
	c_boilElement3.Deactivate();
	// ---------------------
}

void MashTurnOff(){
	// { TURN OFF ELEMENTS }
	mashPID.SetMode( MANUAL );
	c_mashElement.Deactivate();
	// ---------------------
}

void HLTFill(){
	c_hltDisplay.writeDigitAscii(0, 'F');
	c_hltDisplay.writeDigitAscii(1, 'I');
	c_hltDisplay.writeDigitAscii(2, 'L' );
	c_hltDisplay.writeDigitAscii(3, 'L' );
}

void UpdateHLT(){
	if ( c_sysStatus == e_sysStatus_Ready ){
		if ( m_sw_hlt.IsOn() ){
			switch( c_hltStatus ){
			case e_hltStatus_Idle:
				if ( M_brewStartReq ){
					c_hltStatus = e_hltStatus_On;
				} else {
					;//NC
				}		
				break;
					
			case e_hltStatus_On:
				if ( m_hltWaterLevel >= k_hltMinWaterLevel ){
					HLTControlTemp();// {CONTROL TEMPERATURE} 
				} else {
					c_hltStatus = e_hltStatus_Empty;
					HLTTurnOff(); // { TURN OFF HEATING ELEMENTS }
				}
				break;		 
				
			case e_hltStatus_Empty:
				if ( m_hltWaterLevel >= m_hltDesiredWaterLevel ){
					c_hltStatus = e_hltStatus_On;
				} else {
					HLTFill(); // { FILL HLT }
				}
				break;
			}
		} else {
			c_hltStatus = e_hltStatus_Idle;
			HLTTurnOff(); // { TURN OFF HEATING ELEMENTS }
		}
	} else {
		HLTTurnOff(); // { TURN OFF HEATING ELEMENTS }
	}
}

void UpdateMash(){
	if( c_sysStatus == e_sysStatus_Ready ){
		if( m_sw_mash.IsOn() ){
			switch( c_mashStatus ){
			case e_mashStatus_Idle:
				if( M_brewStartReq ){
					c_mashStatus = e_mashStatus_StartReq;
					f_hltSetpoint = recipe_strikeTemp;
				}
				break;
				
			case e_mashStatus_StartReq:
				if( m_temp_hlt->GetTemp() > ( recipe_strikeTemp - hltTempRange ) && m_temp_hlt->GetTemp() < ( recipe_strikeTemp + hltTempRange ) ){
					c_mashStatus = e_mashStatus_MashInReq;
				}
				break;
				
			case e_mashStatus_MashInReq:
				break;
				
			case e_mashStatus_MashIn:
				break;
			
			case e_mashStatus_Rest:
				break;
			case e_mashStatus_MashOut:
				break;
			case e_mashStatus_PreSparge:
				break;
			case e_mashStatus_Sparge:
				break;
			default: break;
			
			}
		}
	}
}


void loop()
{
	//UpdateMenu();
	
	if( UpdateMenu() ){
		WriteMenu();
	}
	
	// MAIN TIMER
	if( timeStatus() == timeSet ){
		c_mainDisplay.writeDigitNum(0, hour()/10 );
		c_mainDisplay.writeDigitNum(1, hour()%10 );
		c_mainDisplay.writeColon( );
		c_mainDisplay.writeDigitNum(2, minute()/10 );
		c_mainDisplay.writeDigitNum(3, minute()%10 );
		} else {
		c_mainDisplay.println( 1111 );
		c_mainDisplay.writeColon( );
	}
		
	if( screenUpdateFlag ){
		// write it out!
		c_hltDisplay.writeDisplay();
		c_mashDisplay.writeDisplay();
		c_boilDisplay.writeDisplay();
		c_mainDisplay.writeDisplay();
		
		c_mainQuadDisplay1.writeDisplay();
		c_mainQuadDisplay2.writeDisplay();
		screenUpdateFlag = false;
	}
	
	// update pids with current input values and perform computation. 
	hltPIDInput = m_temp_hlt->GetTemp();
	hltPID.Compute();
	
	mashPIDInput = m_temp_mash->GetTemp();
	mashPID.Compute();
		
	boilPIDInput = m_temp_boil->GetTemp();
	boilPID.Compute();
			
	// Check if the relay window needs to be shifted
	if(millis() - pidWindowStartTime > pidWindowSize) { 
		pidWindowStartTime += pidWindowSize;
	}
						
	  
	// Power status Check
	UpdateSystemStatus();
	//UpdateHLT();
	//UpdateMash();
	//UpdateBoil();
	
	/*
	if( m_sw_boil.IsOn() ){
		if( !c_boilElement1.IsActive()){
			//Serial.println("Boil activate");
			c_boilElement1.Activate();
		}
	} else if ( c_boilElement1.IsActive() ){
		c_boilElement1.Deactivate();
		//Serial.println("Boil deactivate");
	}
	
	
	if( m_sw_hlt.IsOn() ){
		if( !c_hltElement1.IsActive()){
			//Serial.println("hlt activate");
			c_hltElement1.Activate();
		}
	} else if (c_hltElement1.IsActive() ){ 
		c_hltElement1.Deactivate();
		//Serial.println("hlt deactivate");
	}*/
	
	// PID stuff
	//boilPIDInput = tempSensor[0].GetTemp();
	//boilPID.Compute();
	
	// turn the output pin on/off based on pid output
	//if(millis() - windowStartTime > boilWindowSize)
	//{ //time to shift the Relay Window
//		windowStartTime += boilWindowSize;
//	}
//	if(boilPIDOutput < millis() - windowStartTime) c_boilElement1.Activate();
//	else c_boilElement1.Deactivate();
	
	//------
	
	if( c_sysStatus == e_sysStatus_Ready ){
	
		// HLT SWITCH ON
		if( m_sw_hlt.IsOn() ){
			
			HLTControlTemp();
			
			// write temperature
			int val = int((m_temp_hlt->GetTemp() * 10) + 0.5) ;
		
			if ( val < 100 ){
				c_hltDisplay.writeDigitAscii(0, ' ');
				c_hltDisplay.writeDigitAscii(1, ' ');
				c_hltDisplay.writeDigitAscii(2, val/10 + 48, true );
				c_hltDisplay.writeDigitAscii(3, val%10 + 48 );

			} else if ( val < 1000 ){
				c_hltDisplay.writeDigitAscii(0, ' ');
				c_hltDisplay.writeDigitAscii(1, val/100 + 48 );
				c_hltDisplay.writeDigitAscii(2, (val%100)/10 + 48, true );
				c_hltDisplay.writeDigitAscii(3, val%10 + 48 );

			} else if ( val < 10000 ){
				c_hltDisplay.writeDigitAscii(0, val/1000 + 48 );
				c_hltDisplay.writeDigitAscii(1, (val%1000)/100 + 48 );
				c_hltDisplay.writeDigitAscii(2, (val%100)/10 + 48, true );
				c_hltDisplay.writeDigitAscii(3, val%10 + 48 );
			}

		} else {
			HLTTurnOff();

		// write idle
			c_hltDisplay.writeDigitAscii(0, 'I');
			c_hltDisplay.writeDigitAscii(1, 'D');
			c_hltDisplay.writeDigitAscii(2, 'L');
			c_hltDisplay.writeDigitAscii(3, 'E');

		}
	
		// BOIL SWITCH ON
		if( m_sw_boil.IsOn() ){
			
			BoilControlTemp();
		
			// write temperature
			int val = int((m_temp_boil->GetTemp() * 10) + 0.5) ;
		
			if ( val < 100 ){
				c_boilDisplay.writeDigitAscii(0, ' ');
				c_boilDisplay.writeDigitAscii(1, ' ');
				c_boilDisplay.writeDigitAscii(1, val/10 + 48, true );
				c_boilDisplay.writeDigitAscii(2, val%10 + 48 );

			
				} else if ( val < 1000 ){
				c_boilDisplay.writeDigitAscii(0, ' ');
				c_boilDisplay.writeDigitAscii(1, val/100 + 48 );
				c_boilDisplay.writeDigitAscii(2, (val%100)/10 + 48, true );
				c_boilDisplay.writeDigitAscii(3, val%10 + 48 );

			
				} else if ( val < 10000 ){
				c_boilDisplay.writeDigitAscii(0, val/1000 + 48 );
				c_boilDisplay.writeDigitAscii(1, (val%1000)/100 + 48 );
				c_boilDisplay.writeDigitAscii(2, (val%100)/10 + 48, true );
				c_boilDisplay.writeDigitAscii(3, val%10 + 48 );

			}

			} else {
				
				BoilTurnOff();
				
				c_boilDisplay.writeDigitAscii(0, 'I');
				c_boilDisplay.writeDigitAscii(1, 'D');
				c_boilDisplay.writeDigitAscii(2, 'L');
				c_boilDisplay.writeDigitAscii(3, 'E');

		}
	
		// MASH SWITCH ON
		if( m_sw_mash.IsOn() ){
			
			MashControlTemp();
		
			// set every digit to the buffer
			int val = int((m_temp_mash->GetTemp() * 10) + 0.5) ;
		
			if ( val < 100 ){
				c_mashDisplay.writeDigitAscii(0, ' ');
				c_mashDisplay.writeDigitAscii(1, ' ');
				c_mashDisplay.writeDigitAscii(2, val/10 + 48, true );
				c_mashDisplay.writeDigitAscii(3, val%10 + 48 );

			
				} else if ( val < 1000 ){
				c_mashDisplay.writeDigitAscii(0, ' ');
				c_mashDisplay.writeDigitAscii(1, val/100 + 48 );
				c_mashDisplay.writeDigitAscii(2, (val%100)/10 + 48, true );
				c_mashDisplay.writeDigitAscii(3, val%10 + 48 );

			
				} else if ( val < 10000 ){
				c_mashDisplay.writeDigitAscii(0, val/1000 + 48 );
				c_mashDisplay.writeDigitAscii(1, (val%1000)/100 + 48 );
				c_mashDisplay.writeDigitAscii(2, (val%100)/10 + 48, true );
				c_mashDisplay.writeDigitAscii(3, val%10 + 48 );
			}

			} else {
			//DEACTIVATE ELEMENTS
			MashTurnOff();
		
			// set every digit to the buffer
			c_mashDisplay.writeDigitAscii(0, 'I');
			c_mashDisplay.writeDigitAscii(1, 'D');
			c_mashDisplay.writeDigitAscii(2, 'L');
			c_mashDisplay.writeDigitAscii(3, 'E');
		}
	
		// WATER PUMP SWITCH ON
		if( m_sw_waterPump.IsOn() ){
			c_waterPump.Activate();
		} else {
			c_waterPump.Deactivate();
		}
	
		// WORT PUMP SWITCH ON
		if( m_sw_wortPump.IsOn() ){
			c_wortPump.Activate();	
		} else {
			c_wortPump.Deactivate();
		}
	
		// GLYCOL PUMP
		if( m_sw_glycolPump.IsOn() ){
			c_glycolPump.Activate();
		} else {
			c_glycolPump.Deactivate();
		}
			
		// TRANSFER PUMP
		c_transferPump.Activate();
	
	
		// OUTLET 110V
		if( m_sw_outlet110.IsOn() ){
			c_outlet110.Activate();
		
		} else {
			c_outlet110.Deactivate();
		}
	
		// OUTLET 240V
		if( m_sw_outlet240.IsOn() ){
			c_outlet240.Activate();
		
		} else {
			c_outlet240.Deactivate();
		}
	} else {
		
		HLTTurnOff();
		MashTurnOff();
		BoilTurnOff();
				
		c_wortPump.Deactivate();
		c_waterPump.Deactivate();
		c_glycolPump.Deactivate();
		c_transferPump.Deactivate();
		c_outlet240.Deactivate();
		c_outlet110.Deactivate();
		
		// HLT
		c_hltDisplay.writeDigitAscii(0, 'I');
		c_hltDisplay.writeDigitAscii(1, 'D');
		c_hltDisplay.writeDigitAscii(2, 'L');
		c_hltDisplay.writeDigitAscii(3, 'E');
			
		// MASH
		c_mashDisplay.writeDigitAscii(0, 'I');
		c_mashDisplay.writeDigitAscii(1, 'D');
		c_mashDisplay.writeDigitAscii(2, 'L');
		c_mashDisplay.writeDigitAscii(3, 'E');
			
		// BOIL
		c_boilDisplay.writeDigitAscii(0, 'I');
		c_boilDisplay.writeDigitAscii(1, 'D');
		c_boilDisplay.writeDigitAscii(2, 'L');
		c_boilDisplay.writeDigitAscii(3, 'E');
	}
			
	// listen for incoming clients
	EthernetClient client = server.available();
	if (client) {
		//Serial.println("new client");
		// an http request ends with a blank line
		boolean currentLineIsBlank = true;
		while (client.connected()) {
			if (client.available()) {
				char c = client.read();
			//	Serial.write(c);
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
		//Serial.println("client disconnected");
	}
	

}

void InitDisplays(){
	
	// ASSIGN ADDRESSES
	c_hltDisplay.begin( 0x72 );
	c_mashDisplay.begin( 0x73 );
	c_boilDisplay.begin( 0x74 );
	c_mainQuadDisplay1.begin( 0x71 );
	c_mainQuadDisplay2.begin( 0x75 );
	c_mainDisplay.begin( 0x70 );
		
	// WRITE INITIAL TEXT
	// HLT
	c_hltDisplay.writeDigitAscii(0, 'H');
	c_hltDisplay.writeDigitAscii(1, 'L');
	c_hltDisplay.writeDigitAscii(2, 'T');
	c_hltDisplay.writeDigitAscii(3, ' ');
		
	// MASH
	c_mashDisplay.writeDigitAscii(0, 'M');
	c_mashDisplay.writeDigitAscii(1, 'A');
	c_mashDisplay.writeDigitAscii(2, 'S');
	c_mashDisplay.writeDigitAscii(3, 'H');
	
	// BOIL
	c_boilDisplay.writeDigitAscii(0, 'B');
	c_boilDisplay.writeDigitAscii(1, 'O');
	c_boilDisplay.writeDigitAscii(2, 'I');
	c_boilDisplay.writeDigitAscii(3, 'L');
	
	// MAIN QUADS
	c_mainQuadDisplay1.writeDigitAscii(0, 'W');
	c_mainQuadDisplay1.writeDigitAscii(1, 'E');
	c_mainQuadDisplay1.writeDigitAscii(2, 'L');
	c_mainQuadDisplay1.writeDigitAscii(3, 'C');
	c_mainQuadDisplay2.writeDigitAscii(0, 'O');
	c_mainQuadDisplay2.writeDigitAscii(1, 'M');
	c_mainQuadDisplay2.writeDigitAscii(2, 'E');
	c_mainQuadDisplay2.writeDigitAscii(3, '!');
	
	// MAIN TIMER
	if( timeStatus() == timeSet ){
		c_mainDisplay.writeDigitNum(0, hour()/10 );
		c_mainDisplay.writeDigitNum(1, hour()%10 );
		c_mainDisplay.drawColon( true );
		c_mainDisplay.writeDigitNum(2, minute()/10 );
		c_mainDisplay.writeDigitNum(3, minute()%10 );
	} else {
		c_mainDisplay.println( 9999 );
		c_mainDisplay.drawColon( true );	
	}
	
	// write it out!
	c_hltDisplay.writeDisplay();
	c_mashDisplay.writeDisplay();
	c_boilDisplay.writeDisplay();
	c_mainQuadDisplay1.writeDisplay();
	c_mainQuadDisplay2.writeDisplay();
	c_mainDisplay.writeDisplay();
		
}

void SetTempResolution( OneWire myds ) {
	myds.reset();
	myds.skip(); // skip rom
	myds.write(0x3F);
}

void ISR_TempTimer( ){
	ReadTemperatureSensors();
	StartTempConversion();
	screenUpdateFlag = true; 
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
			//Serial.println("CRC is not valid!");
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
	
	//Serial.println(" ");
	//Serial.print("Number of Sensors: ");
	//Serial.println(TempSensor::GetNumberOfSensors());
	
	for( i = 0; i <  TempSensor::GetNumberOfSensors(); i++ ){
		
		tempSensor[i].SetPresence( TempSensorPresent( &tempSensor[i]));
		
		if ( tempSensor[i].IsPresent() ){
			
			UpdateTempSensor( &tempSensor[i] );
			//Serial.print( tempSensor[i].GetName() );
			//Serial.print(" temperature ");
			//Serial.print( tempSensor[i].GetTemp() );
			//Serial.println(" Celcius.");

			} else {
			//Serial.print( tempSensor[i].GetName() );
			//Serial.println(" NOT found" );
		}
	}
	return;
}


void UpdateTempSensor( TempSensor * sensor ){
	byte i;
	byte data[12];
	byte addr[8];
	byte type_s;
	float celsius;
	
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


// returns true if the screens are changed and need to be written to
bool UpdateMenu(){
	switch( c_sysStatus ){
	case e_sysStatus_Ready:
	
		switch( c_menuScreen ){
		case e_menuScreen_Idle:
			if( m_btn_menuLeft.ShortPressed() ) ;
			else if( m_btn_menuRight.ShortPressed() ) ;//NC
			else if( m_btn_menuLeft.LongPressed() ) ;//NC
			else if( m_btn_menuRight.LongPressed() ){ c_menuScreen = e_menuScreen_Temp; return true; }//NC
			break;
		
		case e_menuScreen_Temp:
			if( m_btn_menuRight.ShortPressed() ) ;//NC
			else if( m_btn_menuLeft.ShortPressed() ) ;//NC
			else if( m_btn_menuRight.LongPressed() ) { c_menuScreen = e_menuScreen_Temp_HLT; return true; }
			else if( m_btn_menuLeft.LongPressed() ) ;//NC
			else if( abs(millis() - m_btn_menuLeft.LastPressTime()) > 10000 && abs(millis() - m_btn_menuRight.LastPressTime()) > 10000 ) { c_menuScreen = e_menuScreen_Idle; return true; }
			break;
		
		case e_menuScreen_Temp_HLT:
			if( m_btn_menuRight.ShortPressed() ){ c_menuScreen = e_menuScreen_Temp_Mash; return true; }
			else if( m_btn_menuLeft.ShortPressed() ){  c_menuScreen = e_menuScreen_Temp_Boil; return true; }
			else if( m_btn_menuRight.LongPressed() ){ c_menuScreen = e_menuScreen_Temp_HLT_Set; return true; }
			else if( m_btn_menuLeft.LongPressed() ){ c_menuScreen = e_menuScreen_Temp; return true; }
			else if( abs(millis() - m_btn_menuLeft.LastPressTime()) > 10000 && abs(millis() - m_btn_menuRight.LastPressTime()) > 10000 ){ c_menuScreen = e_menuScreen_Idle; return true; }
			break;
			
		case e_menuScreen_Temp_Mash:
			if( m_btn_menuRight.ShortPressed() ){ c_menuScreen = e_menuScreen_Temp_Boil; return true; }
			else if( m_btn_menuLeft.ShortPressed() ){ c_menuScreen = e_menuScreen_Temp_HLT; return true; }
			else if( m_btn_menuRight.LongPressed() ){ c_menuScreen = e_menuScreen_Temp_Mash_Set; return true; }
			else if( m_btn_menuLeft.LongPressed() ){ c_menuScreen = e_menuScreen_Temp; return true; } 
			else if( abs(millis() - m_btn_menuLeft.LastPressTime()) > 10000 && abs(millis() - m_btn_menuRight.LastPressTime()) > 10000 ){ c_menuScreen = e_menuScreen_Idle; return true; }
			break;
			
		case e_menuScreen_Temp_Boil:
			if( m_btn_menuRight.ShortPressed() ){ c_menuScreen = e_menuScreen_Temp_HLT; return true; }
			else if( m_btn_menuLeft.ShortPressed() ){ c_menuScreen = e_menuScreen_Temp_Mash; return true; }
			else if( m_btn_menuRight.LongPressed() ){ c_menuScreen = e_menuScreen_Temp_Boil_Set; return true; }
			else if( m_btn_menuLeft.LongPressed() ){ c_menuScreen = e_menuScreen_Temp; return true; }
			else if( abs(millis() - m_btn_menuLeft.LastPressTime()) > 10000 && abs(millis() - m_btn_menuRight.LastPressTime()) > 10000 ){ c_menuScreen = e_menuScreen_Idle; return true; }
			break;
			
		case e_menuScreen_Temp_HLT_Set:
			if( m_btn_menuRight.ShortPressed() ){ hltPIDSetpoint++;  return true; }
			else if( m_btn_menuLeft.ShortPressed() ){ hltPIDSetpoint--; return true; }
			else if( m_btn_menuRight.LongPressed() ) ; //NC
			else if( m_btn_menuLeft.LongPressed() ){ c_menuScreen = e_menuScreen_Temp; return true; }
			else if( abs(millis() - m_btn_menuLeft.LastPressTime()) > 10000 && abs(millis() - m_btn_menuRight.LastPressTime()) > 10000 ){ c_menuScreen = e_menuScreen_Idle; return true; }
			break;	
			
		case e_menuScreen_Temp_Mash_Set:
			if( m_btn_menuRight.ShortPressed() ){ mashPIDSetpoint++; return true; }
			else if( m_btn_menuLeft.ShortPressed() ){ mashPIDSetpoint--; return true; }
			else if( m_btn_menuRight.LongPressed() ) ; //NC
			else if( m_btn_menuLeft.LongPressed() ){ c_menuScreen = e_menuScreen_Temp; return true; }
			else if( abs(millis() - m_btn_menuLeft.LastPressTime()) > 10000 && abs(millis() - m_btn_menuRight.LastPressTime()) > 10000 ){ c_menuScreen = e_menuScreen_Idle; return true; }
			break;
		
		case e_menuScreen_Temp_Boil_Set:
			if( m_btn_menuRight.ShortPressed() ){ boilPIDOutput++; return true; }
			else if( m_btn_menuLeft.ShortPressed() ){ boilPIDSetpoint--; return true; }
			else if( m_btn_menuRight.LongPressed() ) ; //NC
			else if( m_btn_menuLeft.LongPressed() ){ c_menuScreen = e_menuScreen_Temp; return true; }
			else if( abs(millis() - m_btn_menuLeft.LastPressTime()) > 10000 && abs(millis() - m_btn_menuRight.LastPressTime()) > 10000 ){ c_menuScreen = e_menuScreen_Idle; return true; }
			break;
	
		default:
			c_menuScreen = e_menuScreen_Idle;	
			return true;
		}
		break;
	
	
	case e_sysStatus_Emergency:
		c_menuScreen = e_menuScreen_Emergency;
		return true;
		break;
		
	
	case e_sysStatus_Standby:
		c_menuScreen = e_menuScreen_Standby;
		return true;
		break;
	
	default: break;
	}
	return false;
}

void WriteMenu(){
	int val = 0;
	
	switch( c_menuScreen ){
		case e_menuScreen_Idle:
			c_mainQuadDisplay1.writeDigitAscii(0, 'R');
			c_mainQuadDisplay1.writeDigitAscii(1, 'E');
			c_mainQuadDisplay1.writeDigitAscii(2, 'A');
			c_mainQuadDisplay1.writeDigitAscii(3, 'D');
			c_mainQuadDisplay2.writeDigitAscii(0, 'Y');
			c_mainQuadDisplay2.writeDigitAscii(1, ' ');
			c_mainQuadDisplay2.writeDigitAscii(2, ' ');
			c_mainQuadDisplay2.writeDigitAscii(3, ' ');
			break;
			
			
		case e_menuScreen_Temp:
			c_mainQuadDisplay1.writeDigitAscii(0, 'T');
			c_mainQuadDisplay1.writeDigitAscii(1, 'E');
			c_mainQuadDisplay1.writeDigitAscii(2, 'M');
			c_mainQuadDisplay1.writeDigitAscii(3, 'P');
			c_mainQuadDisplay2.writeDigitAscii(0, 'S');
			c_mainQuadDisplay2.writeDigitAscii(1, ' ');
			c_mainQuadDisplay2.writeDigitAscii(2, ' ');
			c_mainQuadDisplay2.writeDigitAscii(3, ' ');
			break;
			
			
		case e_menuScreen_Temp_HLT:
			c_mainQuadDisplay1.writeDigitAscii(0, 'H');
			c_mainQuadDisplay1.writeDigitAscii(1, 'L');
			c_mainQuadDisplay1.writeDigitAscii(2, 'T');
			c_mainQuadDisplay1.writeDigitAscii(3, ' ');
			c_mainQuadDisplay2.writeDigitAscii(0, ' ');
			c_mainQuadDisplay2.writeDigitAscii(1, ' ');
			c_mainQuadDisplay2.writeDigitAscii(2, ' ');
			c_mainQuadDisplay2.writeDigitAscii(3, ' ');
			break;
			
			
		case e_menuScreen_Temp_Mash:
			c_mainQuadDisplay1.writeDigitAscii(0, 'M');
			c_mainQuadDisplay1.writeDigitAscii(1, 'A');
			c_mainQuadDisplay1.writeDigitAscii(2, 'S');
			c_mainQuadDisplay1.writeDigitAscii(3, 'H');
			c_mainQuadDisplay2.writeDigitAscii(0, ' ');
			c_mainQuadDisplay2.writeDigitAscii(1, ' ');
			c_mainQuadDisplay2.writeDigitAscii(2, ' ');
			c_mainQuadDisplay2.writeDigitAscii(3, ' ');
			break;
			
			
		case e_menuScreen_Temp_Boil:
			c_mainQuadDisplay1.writeDigitAscii(0, 'B');
			c_mainQuadDisplay1.writeDigitAscii(1, 'O');
			c_mainQuadDisplay1.writeDigitAscii(2, 'I');
			c_mainQuadDisplay1.writeDigitAscii(3, 'L');
			c_mainQuadDisplay2.writeDigitAscii(0, ' ');
			c_mainQuadDisplay2.writeDigitAscii(1, ' ');
			c_mainQuadDisplay2.writeDigitAscii(2, ' ');
			c_mainQuadDisplay2.writeDigitAscii(3, ' ');
			break;
			
			
		case e_menuScreen_Temp_HLT_Set:
			c_mainQuadDisplay1.writeDigitAscii(0, 'H');
			c_mainQuadDisplay1.writeDigitAscii(1, 'L');
			c_mainQuadDisplay1.writeDigitAscii(2, 'T');
			c_mainQuadDisplay1.writeDigitAscii(3, ':');			
			
			// write out the setpoint
			val = int((hltPIDSetpoint * 10) + 0.5) ;

			if ( val < 100 ){
				c_mainQuadDisplay2.writeDigitAscii(0, ' ');
				c_mainQuadDisplay2.writeDigitAscii(1, ' ');
				c_mainQuadDisplay2.writeDigitAscii(2, val/10 + 48, true );
				c_mainQuadDisplay2.writeDigitAscii(3, val%10 + 48 );

			} else if ( val < 1000 ){
				c_mainQuadDisplay2.writeDigitAscii(0, ' ');
				c_mainQuadDisplay2.writeDigitAscii(1, val/100 + 48 );
				c_mainQuadDisplay2.writeDigitAscii(2, (val%100)/10 + 48, true );
				c_mainQuadDisplay2.writeDigitAscii(3, val%10 + 48 );
		
			} else if ( val < 10000 ){
				c_mainQuadDisplay2.writeDigitAscii(0, val/1000 + 48 );
				c_mainQuadDisplay2.writeDigitAscii(1, (val%1000)/100 + 48 );
				c_mainQuadDisplay2.writeDigitAscii(2, (val%100)/10 + 48, true );
				c_mainQuadDisplay2.writeDigitAscii(3, val%10 + 48 );
			}
			break;
			
			
		case e_menuScreen_Temp_Mash_Set:
			c_mainQuadDisplay1.writeDigitAscii(0, 'M');
			c_mainQuadDisplay1.writeDigitAscii(1, 'A');
			c_mainQuadDisplay1.writeDigitAscii(2, 'S');
			c_mainQuadDisplay1.writeDigitAscii(3, 'H');
			c_mainQuadDisplay2.writeDigitAscii(0, ':');
			
			// write out the setpoint
			val = int((mashPIDSetpoint * 10) + 0.5) ;

			if ( val < 100 ){
				c_mainQuadDisplay2.writeDigitAscii(1, ' ');
				c_mainQuadDisplay2.writeDigitAscii(2, val/10 + 48, true );
				c_mainQuadDisplay2.writeDigitAscii(3, val%10 + 48 );
				c_mainQuadDisplay2.writeDigitAscii(0, ':');

				} else if ( val < 1000 ){
				c_mainQuadDisplay2.writeDigitAscii(1, val/100 + 48 );
				c_mainQuadDisplay2.writeDigitAscii(2, (val%100)/10 + 48, true );
				c_mainQuadDisplay2.writeDigitAscii(3, val%10 + 48 );
				
				} else if ( val < 10000 ){
				c_mainQuadDisplay2.writeDigitAscii(1, val/1000 + 48 );
				c_mainQuadDisplay2.writeDigitAscii(2, (val%1000)/100 + 48 );
				c_mainQuadDisplay2.writeDigitAscii(3, (val%100)/10 + 48 );
			}
			break;
			
			
		case e_menuScreen_Temp_Boil_Set:
			c_mainQuadDisplay1.writeDigitAscii(0, 'B');
			c_mainQuadDisplay1.writeDigitAscii(1, 'O');
			c_mainQuadDisplay1.writeDigitAscii(2, 'I');
			c_mainQuadDisplay1.writeDigitAscii(3, 'L');
			c_mainQuadDisplay2.writeDigitAscii(0, ':');
			
			// write out the setpoint
			val = int((boilPIDSetpoint * 10) + 0.5) ;

			if ( val < 100 ){
				c_mainQuadDisplay2.writeDigitAscii(1, ' ');
				c_mainQuadDisplay2.writeDigitAscii(2, val/10 + 48, true );
				c_mainQuadDisplay2.writeDigitAscii(3, val%10 + 48 );

				} else if ( val < 1000 ){
				c_mainQuadDisplay2.writeDigitAscii(1, val/100 + 48 );
				c_mainQuadDisplay2.writeDigitAscii(2, (val%100)/10 + 48, true );
				c_mainQuadDisplay2.writeDigitAscii(3, val%10 + 48 );
				
				} else if ( val < 10000 ){
				c_mainQuadDisplay2.writeDigitAscii(1, val/1000 + 48 );
				c_mainQuadDisplay2.writeDigitAscii(2, (val%1000)/100 + 48 );
				c_mainQuadDisplay2.writeDigitAscii(3, (val%100)/10 + 48 );
			}
			break;
			
			
		case e_menuScreen_Emergency:
			c_mainQuadDisplay1.writeDigitAscii(0, 'E');
			c_mainQuadDisplay1.writeDigitAscii(1, '-');
			c_mainQuadDisplay1.writeDigitAscii(2, 'S');
			c_mainQuadDisplay1.writeDigitAscii(3, 'T');
			c_mainQuadDisplay2.writeDigitAscii(0, 'O');
			c_mainQuadDisplay2.writeDigitAscii(1, 'P');
			c_mainQuadDisplay2.writeDigitAscii(2, ' ');
			c_mainQuadDisplay2.writeDigitAscii(3, ' ');
			break;
			
			
		case e_menuScreen_Standby:
			c_mainQuadDisplay1.writeDigitAscii(0, 'S');
			c_mainQuadDisplay1.writeDigitAscii(1, 'T');
			c_mainQuadDisplay1.writeDigitAscii(2, 'A');
			c_mainQuadDisplay1.writeDigitAscii(3, 'N');
			c_mainQuadDisplay2.writeDigitAscii(0, 'D');
			c_mainQuadDisplay2.writeDigitAscii(1, 'B');
			c_mainQuadDisplay2.writeDigitAscii(2, 'Y');
			c_mainQuadDisplay2.writeDigitAscii(3, ' ');
			break;
			
			
		default:
			// Error01
			c_mainQuadDisplay1.writeDigitAscii(0, 'E');
			c_mainQuadDisplay1.writeDigitAscii(1, 'R');
			c_mainQuadDisplay1.writeDigitAscii(2, 'R');
			c_mainQuadDisplay1.writeDigitAscii(3, 'O');
			c_mainQuadDisplay2.writeDigitAscii(0, 'R');
			c_mainQuadDisplay2.writeDigitAscii(1, ' ');
			c_mainQuadDisplay2.writeDigitAscii(2, '0');
			c_mainQuadDisplay2.writeDigitAscii(3, '1');
			break;
		
	}
}

/*-------- NTP code ----------*/

/*-------- NTP code ----------*/

time_t getNtpTime()
{
	while (Udp.parsePacket() > 0) ; // discard any previously received packets
	Serial.println("Transmit NTP Request");
	sendNTPpacket(timeServer);
	uint32_t beginWait = millis();
	while (millis() - beginWait < 1500) {
		int size = Udp.parsePacket();
		if (size >= NTP_PACKET_SIZE) {
			Serial.println("Receive NTP Response");
			Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
			unsigned long secsSince1900;
			// convert four bytes starting at location 40 to a long integer
			secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
			secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
			secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
			secsSince1900 |= (unsigned long)packetBuffer[43];
			return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
		}
	}
	Serial.println("No NTP Response :-(");
	return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
	// set all bytes in the buffer to 0
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	// Initialize values needed to form NTP request
	// (see URL above for details on the packets)
	packetBuffer[0] = 0b11100011;   // LI, Version, Mode
	packetBuffer[1] = 0;     // Stratum, or type of clock
	packetBuffer[2] = 6;     // Polling Interval
	packetBuffer[3] = 0xEC;  // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12]  = 49;
	packetBuffer[13]  = 0x4E;
	packetBuffer[14]  = 49;
	packetBuffer[15]  = 52;
	// all NTP fields have been given values, now
	// you can send a packet requesting a timestamp:
	Udp.beginPacket(address, 123); //NTP requests are to port 123
	Udp.write(packetBuffer, NTP_PACKET_SIZE);
	Udp.endPacket();
}