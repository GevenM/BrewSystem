// Recipe.h

#ifndef _RECIPE_h
#define _RECIPE_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

#define k_nameLength 20

typedef char y_name[k_nameLength];
typedef short y_time;
typedef short y_amount;
typedef short y_temp;

typedef enum {
	e_use_boil,
	e_use_dryHop,
	e_use_firstWort,
	e_use_aroma,
} y_use;

typedef enum {
	e_mashType_infusion,
	e_mashType_temperature,
	e_mashType_decoction,
} y_mashType;

typedef struct y_mashStep {
	y_name Name;
	y_mashType Type; // type of mash step
	y_amount InfuseAmount; // amount of water to be infused
	y_temp InfuseTemp; // temperature of water to be infused
	y_time Time; // step duration
	y_temp Temp;	// desired step temperature
	y_time RampTime; // time in min to get to step temp.
} y_mashStep;

typedef struct y_mashProfile {
	y_name Name;
	y_temp SpargeTemp; // temperature of sparge water.
	y_mashStep Step[6];
} y_mashProfile;

typedef struct y_hop{
	y_name Name;
	y_amount Amount;
	y_use Use;
	y_time Time; // Time depends on 'use' see beerxml.
} y_hop;


typedef struct y_fermentable{
	y_name Name;
	y_amount Amount;
} y_fermentable;

typedef struct y_misc{
	y_name Name;
	y_amount Amount;
} y_misc;

typedef struct y_yeast{
	y_name Name;
	y_amount Amount;
} y_yeast;


typedef struct y_recipe {
	y_name Name;
	y_amount BoilSize;
	y_hop Hops[10];
	y_fermentable Fermentables[10];
	y_misc Miscs[10];
	y_yeast Yeasts[3];
	y_mashProfile Mash;
} y_recipe;

#endif

