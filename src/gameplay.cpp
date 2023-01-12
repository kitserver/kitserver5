// gameplay.cpp
#include <windows.h>
#include <stdio.h>
#include "gameplay.h"
#include "kload_exp.h"

KMOD k_gameplay={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;

DWORD protection;
DWORD newProtection = PAGE_EXECUTE_READWRITE;
int counter1;
int counter2;
int conditions[5] = { 0, 1, 2, 3, 4 };

class config_t 
{
public:
    config_t() : autoBallPhysicsEnabled(false), randomFineBallPhysicsEnabled(false), hardpitch_summer_probability(0), 
		hardpitch_winter_probability(0), tweakedConditionArrowsEnabled(false), excellentcondition_replacement("excellent"), 
		goodcondition_replacement("good"), normalcondition_replacement("normal"), poorcondition_replacement("poor"), 
		terriblecondition_replacement("terrible"), additionalSubExtraTimeEnabled(false), ballplayer_proximity(256), ball_gravity(188), 
		ball_globalbouncestick(-28), ball_spinterminationspeed(-376), ball_bouncestickfrequency(-0.00390625), ball_topspin(4551266), 
		ball_airresistance(1400000), ball_touchmoverate(0.003472222248), ball_retardationrate(0.01041666698), player_animspeed(0.0009765625), 
		ballplayer_proximity_hard(256), ball_gravity_hard(188), ball_globalbouncestick_hard(-28), ball_spinterminationspeed_hard(-376), 
		ball_bouncestickfrequency_hard(-0.00390625), ball_topspin_hard(4551266), ball_airresistance_hard(1400000), ball_touchmoverate_hard(0.003472222248), 
		ball_retardationrate_hard(0.01041666698), player_animspeed_hard(0.0009765625), ballplayer_proximity_rain(256), ball_gravity_rain(188), 
		ball_globalbouncestick_rain(-28), ball_spinterminationspeed_rain(-376), ball_bouncestickfrequency_rain(-0.00390625), ball_topspin_rain(4551266), 
		ball_airresistance_rain(1400000), ball_touchmoverate_rain(0.003472222248), ball_retardationrate_rain(0.01041666698), 
		player_animspeed_rain(0.0009765625), ballplayer_proximity_snow(256), ball_gravity_snow(188), ball_globalbouncestick_snow(-28), 
		ball_spinterminationspeed_snow(-376), ball_bouncestickfrequency_snow(-0.00390625), ball_topspin_snow(4551266), 
		ball_airresistance_snow(1400000), ball_touchmoverate_snow(0.003472222248), ball_retardationrate_snow(0.01041666698), 
		player_animspeed_snow(0.0009765625) {}
    bool autoBallPhysicsEnabled;
	bool randomFineBallPhysicsEnabled;
	int hardpitch_summer_probability;
	int hardpitch_winter_probability;
	bool tweakedConditionArrowsEnabled;
	string excellentcondition_replacement;
	string goodcondition_replacement;
	string normalcondition_replacement;
	string poorcondition_replacement;
	string terriblecondition_replacement;
	bool additionalSubExtraTimeEnabled;
	float ballplayer_proximity;
	float ball_gravity;
	float ball_globalbouncestick;
	float ball_spinterminationspeed;
	float ball_bouncestickfrequency;
	float ball_topspin;
	float ball_airresistance;
	float ball_touchmoverate;
	float ball_retardationrate;
	float player_animspeed;
	float ballplayer_proximity_hard;
	float ball_gravity_hard;
	float ball_globalbouncestick_hard;
	float ball_spinterminationspeed_hard;
	float ball_bouncestickfrequency_hard;
	float ball_topspin_hard;
	float ball_airresistance_hard;
	float ball_touchmoverate_hard;
	float ball_retardationrate_hard;
	float player_animspeed_hard;
	float ballplayer_proximity_rain;
	float ball_gravity_rain;
	float ball_globalbouncestick_rain;
	float ball_spinterminationspeed_rain;
	float ball_bouncestickfrequency_rain;
	float ball_topspin_rain;
	float ball_airresistance_rain;
	float ball_touchmoverate_rain;
	float ball_retardationrate_rain;
	float player_animspeed_rain;
	float ballplayer_proximity_snow;
	float ball_gravity_snow;
	float ball_globalbouncestick_snow;
	float ball_spinterminationspeed_snow;
	float ball_bouncestickfrequency_snow;
	float ball_topspin_snow;
	float ball_airresistance_snow;
	float ball_touchmoverate_snow;
	float ball_retardationrate_snow;
	float player_animspeed_snow;
};

config_t gameplay_config;

static void string_strip(string& s)
{
    static const char* empties = " \t\n\r\"";
    int e = s.find_last_not_of(empties);
    s.erase(e + 1);
    int b = s.find_first_not_of(empties);
    s.erase(0,b);
}

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void InitGameplay();
void SetBallPhysicsData();
void SetTweakedConditionArrowsData1();
bool SetTweakedConditionArrowsData2();
bool SetAdditionalSubstitutionData();
bool readConfig(config_t& config);

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_gameplay,"Attaching dll...");

		switch (GetPESInfo()->GameVersion) {
			case gvPES5PC: //support for PES5 PC...
            case gvWE9LEPC: //... and WE9:LE PC
				break;
            default:
                Log(&k_gameplay,"Your game version is currently not supported!");
                return false;
		}
		
		hInst=hInstance;

		RegisterKModule(&k_gameplay);
		
		HookFunction(hk_D3D_Create,(DWORD)InitGameplay);
		HookFunction(hk_UniSplit,(DWORD)SetBallPhysicsData);
		HookFunction(hk_BeginUniSelect,(DWORD)SetTweakedConditionArrowsData1);
		HookFunction(hk_AfterReadFile,(DWORD)SetTweakedConditionArrowsData2);
		HookFunction(hk_AfterReadFile,(DWORD)SetAdditionalSubstitutionData);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_gameplay,"Detaching dll...");

		UnhookFunction(hk_AfterReadFile,(DWORD)SetAdditionalSubstitutionData);
		UnhookFunction(hk_AfterReadFile,(DWORD)SetTweakedConditionArrowsData2);
		UnhookFunction(hk_BeginUniSelect,(DWORD)SetTweakedConditionArrowsData1);
		UnhookFunction(hk_UniSplit,(DWORD)SetBallPhysicsData);

		Log(&k_gameplay,"Detaching done.");
	}
	
	return true;
}

void InitGameplay()
{
	UnhookFunction(hk_D3D_Create,(DWORD)InitGameplay);

	// read configuration
    readConfig(gameplay_config);

	LOG(&k_gameplay, "ReadConfig: excellent-condition.replacement == {%s}", 
    gameplay_config.excellentcondition_replacement.c_str());
	LOG(&k_gameplay, "ReadConfig: good-condition.replacement == {%s}", 
    gameplay_config.goodcondition_replacement.c_str());
	LOG(&k_gameplay, "ReadConfig: normal-condition.replacement == {%s}", 
    gameplay_config.normalcondition_replacement.c_str());
	LOG(&k_gameplay, "ReadConfig: poor-condition.replacement == {%s}", 
    gameplay_config.poorcondition_replacement.c_str());
	LOG(&k_gameplay, "ReadConfig: terrible-condition.replacement == {%s}", 
    gameplay_config.terriblecondition_replacement.c_str());

	switch (GetPESInfo()->GameVersion) {
		case gvWE9LEPC:
			/**
			* ball-player.proximity also controls screen position
			*
			* 0x4198ea // NOPs to be overwritten
			* 0x41991e // screen position detector
			*/
			if (VirtualProtect((LPVOID)0x4198ea, sizeof(0x4198ea), newProtection, &protection)) {
				*(float*)0x4198ea = 256;
				Log(&k_gameplay,"NOPs to be overwritten changed.");
			}
			else {
				Log(&k_gameplay, "Problem changing NOPs to be overwritten.");
			}
			BYTE* bptr = (BYTE*)0x41991e;
			if (VirtualProtect((LPVOID)0x41991e, sizeof(0x41991e), newProtection, &protection)) {
				bptr[2] = 0xea;
				bptr[3] = 0x98;
				bptr[4] = 0x41;
				Log(&k_gameplay,"Screen position detector changed.");
			}
			else {
				Log(&k_gameplay, "Problem changing screen position detector.");
			}
			break;
	}

	if (gameplay_config.excellentcondition_replacement == "excellent")
	{
		conditions[0] = 0;
	}
	else if (gameplay_config.excellentcondition_replacement == "good")
	{
		conditions[0] = 1;
	}
	else if (gameplay_config.excellentcondition_replacement == "normal")
	{
		conditions[0] = 2;
	}
	else if (gameplay_config.excellentcondition_replacement == "poor")
	{
		conditions[0] = 3;
	}
	else if (gameplay_config.excellentcondition_replacement == "terrible")
	{
		conditions[0] = 4;
	}
	if (gameplay_config.goodcondition_replacement == "excellent")
	{
		conditions[1] = 0;
	}
	else if (gameplay_config.goodcondition_replacement == "good")
	{
		conditions[1] = 1;
	}
	else if (gameplay_config.goodcondition_replacement == "normal")
	{
		conditions[1] = 2;
	}
	else if (gameplay_config.goodcondition_replacement == "poor")
	{
		conditions[1] = 3;
	}
	else if (gameplay_config.goodcondition_replacement == "terrible")
	{
		conditions[1] = 4;
	}
	if (gameplay_config.normalcondition_replacement == "excellent")
	{
		conditions[2] = 0;
	}
	else if (gameplay_config.normalcondition_replacement == "good")
	{
		conditions[2] = 1;
	}
	else if (gameplay_config.normalcondition_replacement == "normal")
	{
		conditions[2] = 2;
	}
	else if (gameplay_config.normalcondition_replacement == "poor")
	{
		conditions[2] = 3;
	}
	else if (gameplay_config.normalcondition_replacement == "terrible")
	{
		conditions[2] = 4;
	}
	if (gameplay_config.poorcondition_replacement == "excellent")
	{
		conditions[3] = 0;
	}
	else if (gameplay_config.poorcondition_replacement == "good")
	{
		conditions[3] = 1;
	}
	else if (gameplay_config.poorcondition_replacement == "normal")
	{
		conditions[3] = 2;
	}
	else if (gameplay_config.poorcondition_replacement == "poor")
	{
		conditions[3] = 3;
	}
	else if (gameplay_config.poorcondition_replacement == "terrible")
	{
		conditions[3] = 4;
	}
	if (gameplay_config.terriblecondition_replacement == "excellent")
	{
		conditions[4] = 0;
	}
	else if (gameplay_config.terriblecondition_replacement == "good")
	{
		conditions[4] = 1;
	}
	else if (gameplay_config.terriblecondition_replacement == "normal")
	{
		conditions[4] = 2;
	}
	else if (gameplay_config.terriblecondition_replacement == "poor")
	{
		conditions[4] = 3;
	}
	else if (gameplay_config.terriblecondition_replacement == "terrible")
	{
		conditions[4] = 4;
	}

	Log(&k_gameplay, "Module initialized.");
}

void SetBallPhysicsData()
{
	int pitch;

    float physics1;
	float physics2;
	float physics3;
	float physics4;
	float physics5;
	float physics6;
	float physics7;
	float physics8;
	float physics9;
	float physics10;

	if (gameplay_config.additionalSubExtraTimeEnabled) {
		counter2 = 0;
	}

	switch (GetPESInfo()->GameVersion) {
		case gvPES5PC:
			/**
			* 0x3be0f47 // weather: 0=fine, 1=rainy, 2=snow
			* 0x3beab47 // season: 0=summer, 1=winter
			*/
			if (gameplay_config.autoBallPhysicsEnabled && gameplay_config.randomFineBallPhysicsEnabled && *(BYTE*)0x3be0f47 == 0) {
				srand(GetTickCount());
				pitch = rand() % 100 + 1;
			}
			if (gameplay_config.autoBallPhysicsEnabled && ((gameplay_config.randomFineBallPhysicsEnabled && *(BYTE*)0x3be0f47 == 0 && 
				*(BYTE*)0x3beab47 == 0 && gameplay_config.hardpitch_summer_probability > 0 && 
				pitch <= gameplay_config.hardpitch_summer_probability) || (gameplay_config.randomFineBallPhysicsEnabled && 
				*(BYTE*)0x3be0f47 == 0 && *(BYTE*)0x3beab47 == 1 && gameplay_config.hardpitch_winter_probability > 0 && 
				pitch <= gameplay_config.hardpitch_winter_probability) || (!gameplay_config.randomFineBallPhysicsEnabled && 
				*(BYTE*)0x3be0f47 == 0 && *(BYTE*)0x3beab47 == 1))) {
					physics1 = gameplay_config.ballplayer_proximity_hard;
					physics2 = gameplay_config.ball_gravity_hard;
					physics3 = gameplay_config.ball_globalbouncestick_hard;
					physics4 = gameplay_config.ball_spinterminationspeed_hard;
					physics5 = gameplay_config.ball_bouncestickfrequency_hard;
					physics6 = gameplay_config.ball_topspin_hard;
					physics7 = gameplay_config.ball_airresistance_hard;
					physics8 = gameplay_config.ball_touchmoverate_hard;
					physics9 = gameplay_config.ball_retardationrate_hard;
					physics10 = gameplay_config.player_animspeed_hard;
			}
			else if (gameplay_config.autoBallPhysicsEnabled && *(BYTE*)0x3be0f47 == 1) {
				physics1 = gameplay_config.ballplayer_proximity_rain;
				physics2 = gameplay_config.ball_gravity_rain;
				physics3 = gameplay_config.ball_globalbouncestick_rain;
				physics4 = gameplay_config.ball_spinterminationspeed_rain;
				physics5 = gameplay_config.ball_bouncestickfrequency_rain;
				physics6 = gameplay_config.ball_topspin_rain;
				physics7 = gameplay_config.ball_airresistance_rain;
				physics8 = gameplay_config.ball_touchmoverate_rain;
				physics9 = gameplay_config.ball_retardationrate_rain;
				physics10 = gameplay_config.player_animspeed_rain;
			}
			else if (gameplay_config.autoBallPhysicsEnabled && *(BYTE*)0x3be0f47 == 2) {
				physics1 = gameplay_config.ballplayer_proximity_snow;
				physics2 = gameplay_config.ball_gravity_snow;
				physics3 = gameplay_config.ball_globalbouncestick_snow;
				physics4 = gameplay_config.ball_spinterminationspeed_snow;
				physics5 = gameplay_config.ball_bouncestickfrequency_snow;
				physics6 = gameplay_config.ball_topspin_snow;
				physics7 = gameplay_config.ball_airresistance_snow;
				physics8 = gameplay_config.ball_touchmoverate_snow;
				physics9 = gameplay_config.ball_retardationrate_snow;
				physics10 = gameplay_config.player_animspeed_snow;
			}
			else {
				physics1 = gameplay_config.ballplayer_proximity;
				physics2 = gameplay_config.ball_gravity;
				physics3 = gameplay_config.ball_globalbouncestick;
				physics4 = gameplay_config.ball_spinterminationspeed;
				physics5 = gameplay_config.ball_bouncestickfrequency;
				physics6 = gameplay_config.ball_topspin;
				physics7 = gameplay_config.ball_airresistance;
				physics8 = gameplay_config.ball_touchmoverate;
				physics9 = gameplay_config.ball_retardationrate;
				physics10 = gameplay_config.player_animspeed;
			}

			if (VirtualProtect((LPVOID)0x3be1a6c, sizeof(0x3be1a6c), newProtection, &protection)) {
				*(float*)0x3be1a6c = physics1;
				LogWithDouble(&k_gameplay, "Ball/player proximity: %g", (double)*(float*)0x3be1a6c);
			}
			else {
				Log(&k_gameplay, "Problem changing ball/player proximity.");
			}
			if (VirtualProtect((LPVOID)0xadf8c4, sizeof(0xadf8c4), newProtection, &protection)) {
				*(float*)0xadf8c4 = physics2;
				LogWithDouble(&k_gameplay, "Ball gravity: %g", (double)*(float*)0xadf8c4);
			}
			else {
				Log(&k_gameplay, "Problem changing ball gravity.");
			}
			if (VirtualProtect((LPVOID)0xadf8c8, sizeof(0xadf8c8), newProtection, &protection)) {
				*(float*)0xadf8c8 = physics3;
				LogWithDouble(&k_gameplay, "Ball global bounce/stick: %g", (double)*(float*)0xadf8c8);
			}
			else {
				Log(&k_gameplay, "Problem changing ball global bounce/stick.");
			}
			if (VirtualProtect((LPVOID)0xaeb020, sizeof(0xaeb020), newProtection, &protection)) {
				*(float*)0xaeb020 = physics4;
				LogWithDouble(&k_gameplay, "Ball spin termination speed: %g", (double)*(float*)0xaeb020);
			}
			else {
				Log(&k_gameplay, "Problem changing ball spin termination speed.");
			}
			if (VirtualProtect((LPVOID)0xaeb028, sizeof(0xaeb028), newProtection, &protection)) {
				*(float*)0xaeb028 = physics5;
				LogWithDouble(&k_gameplay, "Ball bounce/stick frequency: %g", (double)*(float*)0xaeb028);
			}
			else {
				Log(&k_gameplay, "Problem changing ball bounce/stick frequency.");
			}
			if (VirtualProtect((LPVOID)0x3be1a78, sizeof(0x3be1a78), newProtection, &protection)) {
				*(float*)0x3be1a78 = physics6;
				LogWithDouble(&k_gameplay, "Ball topspin: %g", (double)*(float*)0x3be1a78);
			}
			else {
				Log(&k_gameplay, "Problem changing ball topspin.");
			}
			if (VirtualProtect((LPVOID)0x3be1a80, sizeof(0x3be1a80), newProtection, &protection)) {
				*(float*)0x3be1a80 = physics7;
				LogWithDouble(&k_gameplay, "Ball air resistance: %g", (double)*(float*)0x3be1a80);
			}
			else {
				Log(&k_gameplay, "Problem changing ball air resistance.");
			}
			if (VirtualProtect((LPVOID)0xaeb014, sizeof(0xaeb014), newProtection, &protection)) {
				*(float*)0xaeb014 = physics8;
				LogWithDouble(&k_gameplay, "Ball touch to move rate: %g", (double)*(float*)0xaeb014);
			}
			else {
				Log(&k_gameplay, "Problem changing ball touch to move rate.");
			}
			if (VirtualProtect((LPVOID)0xaeb018, sizeof(0xaeb018), newProtection, &protection)) {
				*(float*)0xaeb018 = physics9;
				LogWithDouble(&k_gameplay, "Ball retardation rate: %g", (double)*(float*)0xaeb018);
			}
			else {
				Log(&k_gameplay, "Problem changing ball retardation rate.");
			}
			if (VirtualProtect((LPVOID)0xaeb034, sizeof(0xaeb034), newProtection, &protection)) {
				*(float*)0xaeb034 = physics10;
				LogWithDouble(&k_gameplay, "Player anim. speed: %g", (double)*(float*)0xaeb034);
			}
			else {
				Log(&k_gameplay, "Problem changing player anim. speed.");
			}
			break;
		case gvWE9LEPC:
			/**
			* 0x3b68a87 // weather: 0=fine, 1=rainy, 2=snow
			* 0x3b72687 // season: 0=summer, 1=winter
			*/
			if (gameplay_config.autoBallPhysicsEnabled && gameplay_config.randomFineBallPhysicsEnabled && *(BYTE*)0x3b68a87 == 0) {
				srand(GetTickCount());
				pitch = rand() % 100 + 1;
			}
			if (gameplay_config.autoBallPhysicsEnabled && ((gameplay_config.randomFineBallPhysicsEnabled && *(BYTE*)0x3b68a87 == 0 && 
				*(BYTE*)0x3b72687 == 0 && gameplay_config.hardpitch_summer_probability > 0 && 
				pitch <= gameplay_config.hardpitch_summer_probability) || (gameplay_config.randomFineBallPhysicsEnabled && 
				*(BYTE*)0x3b68a87 == 0 && *(BYTE*)0x3b72687 == 1 && gameplay_config.hardpitch_winter_probability > 0 && 
				pitch <= gameplay_config.hardpitch_winter_probability) || (!gameplay_config.randomFineBallPhysicsEnabled && 
				*(BYTE*)0x3b68a87 == 0 && *(BYTE*)0x3b72687 == 1))) {
					physics1 = gameplay_config.ballplayer_proximity_hard;
					physics2 = gameplay_config.ball_gravity_hard;
					physics3 = gameplay_config.ball_globalbouncestick_hard;
					physics4 = gameplay_config.ball_spinterminationspeed_hard;
					physics5 = gameplay_config.ball_bouncestickfrequency_hard;
					physics6 = gameplay_config.ball_topspin_hard;
					physics7 = gameplay_config.ball_airresistance_hard;
					physics8 = gameplay_config.ball_touchmoverate_hard;
					physics9 = gameplay_config.ball_retardationrate_hard;
					physics10 = gameplay_config.player_animspeed_hard;
			}
			else if (gameplay_config.autoBallPhysicsEnabled && *(BYTE*)0x3b68a87 == 1) {
				physics1 = gameplay_config.ballplayer_proximity_rain;
				physics2 = gameplay_config.ball_gravity_rain;
				physics3 = gameplay_config.ball_globalbouncestick_rain;
				physics4 = gameplay_config.ball_spinterminationspeed_rain;
				physics5 = gameplay_config.ball_bouncestickfrequency_rain;
				physics6 = gameplay_config.ball_topspin_rain;
				physics7 = gameplay_config.ball_airresistance_rain;
				physics8 = gameplay_config.ball_touchmoverate_rain;
				physics9 = gameplay_config.ball_retardationrate_rain;
				physics10 = gameplay_config.player_animspeed_rain;
			}
			else if (gameplay_config.autoBallPhysicsEnabled && *(BYTE*)0x3b68a87 == 2) {
				physics1 = gameplay_config.ballplayer_proximity_snow;
				physics2 = gameplay_config.ball_gravity_snow;
				physics3 = gameplay_config.ball_globalbouncestick_snow;
				physics4 = gameplay_config.ball_spinterminationspeed_snow;
				physics5 = gameplay_config.ball_bouncestickfrequency_snow;
				physics6 = gameplay_config.ball_topspin_snow;
				physics7 = gameplay_config.ball_airresistance_snow;
				physics8 = gameplay_config.ball_touchmoverate_snow;
				physics9 = gameplay_config.ball_retardationrate_snow;
				physics10 = gameplay_config.player_animspeed_snow;
			}
			else {
				physics1 = gameplay_config.ballplayer_proximity;
				physics2 = gameplay_config.ball_gravity;
				physics3 = gameplay_config.ball_globalbouncestick;
				physics4 = gameplay_config.ball_spinterminationspeed;
				physics5 = gameplay_config.ball_bouncestickfrequency;
				physics6 = gameplay_config.ball_topspin;
				physics7 = gameplay_config.ball_airresistance;
				physics8 = gameplay_config.ball_touchmoverate;
				physics9 = gameplay_config.ball_retardationrate;
				physics10 = gameplay_config.player_animspeed;
			}

			if (VirtualProtect((LPVOID)0xacfb80, sizeof(0xacfb80), newProtection, &protection)) {
				*(float*)0xacfb80 = physics1;
				LogWithDouble(&k_gameplay, "Ball/player proximity: %g", (double)*(float*)0xacfb80);
			}
			else {
				Log(&k_gameplay, "Problem changing ball/player proximity.");
			}
			if (VirtualProtect((LPVOID)0xadf074, sizeof(0xadf074), newProtection, &protection)) {
				*(float*)0xadf074 = physics2;
				LogWithDouble(&k_gameplay, "Ball gravity: %g", (double)*(float*)0xadf074);
			}
			else {
				Log(&k_gameplay, "Problem changing ball gravity.");
			}
			if (VirtualProtect((LPVOID)0xadf078, sizeof(0xadf078), newProtection, &protection)) {
				*(float*)0xadf078 = physics3;
				LogWithDouble(&k_gameplay, "Ball global bounce/stick: %g", (double)*(float*)0xadf078);
			}
			else {
				Log(&k_gameplay, "Problem changing ball global bounce/stick.");
			}
			if (VirtualProtect((LPVOID)0xaec418, sizeof(0xaec418), newProtection, &protection)) {
				*(float*)0xaec418 = physics4;
				LogWithDouble(&k_gameplay, "Ball spin termination speed: %g", (double)*(float*)0xaec418);
			}
			else {
				Log(&k_gameplay, "Problem changing ball spin termination speed.");
			}
			if (VirtualProtect((LPVOID)0xaec420, sizeof(0xaec420), newProtection, &protection)) {
				*(float*)0xaec420 = physics5;
				LogWithDouble(&k_gameplay, "Ball bounce/stick frequency: %g", (double)*(float*)0xaec420);
			}
			else {
				Log(&k_gameplay, "Problem changing ball bounce/stick frequency.");
			}
			if (VirtualProtect((LPVOID)0x3b695b8, sizeof(0x3b695b8), newProtection, &protection)) {
				*(float*)0x3b695b8 = physics6;
				LogWithDouble(&k_gameplay, "Ball topspin: %g", (double)*(float*)0x3b695b8);
			}
			else {
				Log(&k_gameplay, "Problem changing ball topspin.");
			}
			if (VirtualProtect((LPVOID)0x3b695c0, sizeof(0x3b695c0), newProtection, &protection)) {
				*(float*)0x3b695c0 = physics7;
				LogWithDouble(&k_gameplay, "Ball air resistance: %g", (double)*(float*)0x3b695c0);
			}
			else {
				Log(&k_gameplay, "Problem changing ball air resistance.");
			}
			if (VirtualProtect((LPVOID)0xaec40c, sizeof(0xaec40c), newProtection, &protection)) {
				*(float*)0xaec40c = physics8;
				LogWithDouble(&k_gameplay, "Ball touch to move rate: %g", (double)*(float*)0xaec40c);
			}
			else {
				Log(&k_gameplay, "Problem changing ball touch to move rate.");
			}
			if (VirtualProtect((LPVOID)0xaec410, sizeof(0xaec410), newProtection, &protection)) {
				*(float*)0xaec410 = physics9;
				LogWithDouble(&k_gameplay, "Ball retardation rate: %g", (double)*(float*)0xaec410);
			}
			else {
				Log(&k_gameplay, "Problem changing ball retardation rate.");
			}
			if (VirtualProtect((LPVOID)0xaec42c, sizeof(0xaec42c), newProtection, &protection)) {
				*(float*)0xaec42c = physics10;
				LogWithDouble(&k_gameplay, "Player anim. speed: %g", (double)*(float*)0xaec42c);
			}
			else {
				Log(&k_gameplay, "Problem changing player anim. speed.");
			}
			break;
	}
}

void SetTweakedConditionArrowsData1()
{
	switch (GetPESInfo()->GameVersion) {
		case gvPES5PC:
			if (gameplay_config.tweakedConditionArrowsEnabled) {
				if (VirtualProtect((LPVOID)0x39568d0, sizeof(0x39568d0), newProtection, &protection)) {
					*(BYTE*)0x39568d0 = 255;
				}
				counter1 = 0;
			}
			break;
		case gvWE9LEPC:
			if (gameplay_config.tweakedConditionArrowsEnabled) {
				if (VirtualProtect((LPVOID)0x38b3178, sizeof(0x38b3178), newProtection, &protection)) {
					*(BYTE*)0x38b3178 = 255;
				}
				counter1 = 0;
			}
			break;
	}
}

bool SetTweakedConditionArrowsData2()
{
	switch (GetPESInfo()->GameVersion) {
		case gvPES5PC:
			/**
			* 0x3b7a0f4 // presence in out-of-match menus?: 0=no, 1=yes
			*/
			if (gameplay_config.tweakedConditionArrowsEnabled && *(BYTE*)0x39568d0 < 255 && counter1 == 0 && *(BYTE*)0x3b7a0f4 == 0) {
				if (VirtualProtect((LPVOID)0x39568d0, sizeof(0x39568d0), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568d1, sizeof(0x39568d1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568d2, sizeof(0x39568d2), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568d3, sizeof(0x39568d3), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568d4, sizeof(0x39568d4), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568d5, sizeof(0x39568d5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568d6, sizeof(0x39568d6), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568d7, sizeof(0x39568d7), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568d8, sizeof(0x39568d8), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568d9, sizeof(0x39568d9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568da, sizeof(0x39568da), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568db, sizeof(0x39568db), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568dc, sizeof(0x39568dc), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568dd, sizeof(0x39568dd), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568de, sizeof(0x39568de), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568df, sizeof(0x39568df), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568e0, sizeof(0x39568e0), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568e1, sizeof(0x39568e1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568e2, sizeof(0x39568e2), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568e3, sizeof(0x39568e3), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568e4, sizeof(0x39568e4), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568e5, sizeof(0x39568e5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568e6, sizeof(0x39568e6), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568e7, sizeof(0x39568e7), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568e8, sizeof(0x39568e8), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568e9, sizeof(0x39568e9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568ea, sizeof(0x39568ea), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568eb, sizeof(0x39568eb), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568ec, sizeof(0x39568ec), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568ed, sizeof(0x39568ed), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568ee, sizeof(0x39568ee), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x39568ef, sizeof(0x39568ef), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957474, sizeof(0x3957474), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957475, sizeof(0x3957475), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957476, sizeof(0x3957476), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957477, sizeof(0x3957477), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957478, sizeof(0x3957478), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957479, sizeof(0x3957479), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395747a, sizeof(0x395747a), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395747b, sizeof(0x395747b), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395747c, sizeof(0x395747c), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395747d, sizeof(0x395747d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395747e, sizeof(0x395747e), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395747f, sizeof(0x395747f), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957480, sizeof(0x3957480), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957481, sizeof(0x3957481), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957482, sizeof(0x3957482), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957483, sizeof(0x3957483), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957484, sizeof(0x3957484), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957485, sizeof(0x3957485), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957486, sizeof(0x3957486), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957487, sizeof(0x3957487), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957488, sizeof(0x3957488), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957489, sizeof(0x3957489), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395748a, sizeof(0x395748a), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395748b, sizeof(0x395748b), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395748c, sizeof(0x395748c), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395748d, sizeof(0x395748d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395748e, sizeof(0x395748e), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395748f, sizeof(0x395748f), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957490, sizeof(0x3957490), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957491, sizeof(0x3957491), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957492, sizeof(0x3957492), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3957493, sizeof(0x3957493), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b524, sizeof(0x395b524), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b525, sizeof(0x395b525), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b526, sizeof(0x395b526), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b527, sizeof(0x395b527), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b528, sizeof(0x395b528), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b529, sizeof(0x395b529), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b52a, sizeof(0x395b52a), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b52b, sizeof(0x395b52b), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b52c, sizeof(0x395b52c), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b52d, sizeof(0x395b52d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b52e, sizeof(0x395b52e), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b52f, sizeof(0x395b52f), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b530, sizeof(0x395b530), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b531, sizeof(0x395b531), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b532, sizeof(0x395b532), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b533, sizeof(0x395b533), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b534, sizeof(0x395b534), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b535, sizeof(0x395b535), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b536, sizeof(0x395b536), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b537, sizeof(0x395b537), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b538, sizeof(0x395b538), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b539, sizeof(0x395b539), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b53a, sizeof(0x395b53a), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b53b, sizeof(0x395b53b), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b53c, sizeof(0x395b53c), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b53d, sizeof(0x395b53d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b53e, sizeof(0x395b53e), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b53f, sizeof(0x395b53f), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b540, sizeof(0x395b540), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b541, sizeof(0x395b541), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b542, sizeof(0x395b542), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395b543, sizeof(0x395b543), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0c8, sizeof(0x395c0c8), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0c9, sizeof(0x395c0c9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0ca, sizeof(0x395c0ca), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0cb, sizeof(0x395c0cb), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0cc, sizeof(0x395c0cc), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0cd, sizeof(0x395c0cd), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0ce, sizeof(0x395c0ce), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0cf, sizeof(0x395c0cf), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0d0, sizeof(0x395c0d0), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0d1, sizeof(0x395c0d1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0d2, sizeof(0x395c0d2), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0d3, sizeof(0x395c0d3), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0d4, sizeof(0x395c0d4), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0d5, sizeof(0x395c0d5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0d6, sizeof(0x395c0d6), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0d7, sizeof(0x395c0d7), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0d8, sizeof(0x395c0d8), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0d9, sizeof(0x395c0d9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0da, sizeof(0x395c0da), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0db, sizeof(0x395c0db), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0dc, sizeof(0x395c0dc), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0dd, sizeof(0x395c0dd), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0de, sizeof(0x395c0de), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0df, sizeof(0x395c0df), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0e0, sizeof(0x395c0e0), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0e1, sizeof(0x395c0e1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0e2, sizeof(0x395c0e2), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0e3, sizeof(0x395c0e3), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0e4, sizeof(0x395c0e4), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0e5, sizeof(0x395c0e5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0e6, sizeof(0x395c0e6), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x395c0e7, sizeof(0x395c0e7), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd3dd5, sizeof(0x3bd3dd5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd4119, sizeof(0x3bd4119), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd445d, sizeof(0x3bd445d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd47a1, sizeof(0x3bd47a1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd4ae5, sizeof(0x3bd4ae5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd4e29, sizeof(0x3bd4e29), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd516d, sizeof(0x3bd516d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd54b1, sizeof(0x3bd54b1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd57f5, sizeof(0x3bd57f5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd5b39, sizeof(0x3bd5b39), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd5e7d, sizeof(0x3bd5e7d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd61c1, sizeof(0x3bd61c1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd6505, sizeof(0x3bd6505), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd6849, sizeof(0x3bd6849), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd6b8d, sizeof(0x3bd6b8d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd6ed1, sizeof(0x3bd6ed1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd7215, sizeof(0x3bd7215), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd7559, sizeof(0x3bd7559), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd789d, sizeof(0x3bd789d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd7be1, sizeof(0x3bd7be1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd7f25, sizeof(0x3bd7f25), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd8269, sizeof(0x3bd8269), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd85ad, sizeof(0x3bd85ad), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd88f1, sizeof(0x3bd88f1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd8c35, sizeof(0x3bd8c35), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd8f79, sizeof(0x3bd8f79), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd92bd, sizeof(0x3bd92bd), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd9601, sizeof(0x3bd9601), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd9945, sizeof(0x3bd9945), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd9c89, sizeof(0x3bd9c89), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bd9fcd, sizeof(0x3bd9fcd), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bda311, sizeof(0x3bda311), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bda655, sizeof(0x3bda655), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bda999, sizeof(0x3bda999), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdacdd, sizeof(0x3bdacdd), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdb021, sizeof(0x3bdb021), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdb365, sizeof(0x3bdb365), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdb6a9, sizeof(0x3bdb6a9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdb9ed, sizeof(0x3bdb9ed), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdbd31, sizeof(0x3bdbd31), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdc075, sizeof(0x3bdc075), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdc3b9, sizeof(0x3bdc3b9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdc6fd, sizeof(0x3bdc6fd), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdca41, sizeof(0x3bdca41), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdcd85, sizeof(0x3bdcd85), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdd0c9, sizeof(0x3bdd0c9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdd40d, sizeof(0x3bdd40d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdd751, sizeof(0x3bdd751), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdda95, sizeof(0x3bdda95), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdddd9, sizeof(0x3bdddd9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bde11d, sizeof(0x3bde11d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bde461, sizeof(0x3bde461), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bde7a5, sizeof(0x3bde7a5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdeae9, sizeof(0x3bdeae9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdee2d, sizeof(0x3bdee2d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdf171, sizeof(0x3bdf171), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdf4b5, sizeof(0x3bdf4b5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdf7f9, sizeof(0x3bdf7f9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdfb3d, sizeof(0x3bdfb3d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3bdfe81, sizeof(0x3bdfe81), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3be01c5, sizeof(0x3be01c5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3be0509, sizeof(0x3be0509), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3be084d, sizeof(0x3be084d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3be0b91, sizeof(0x3be0b91), newProtection, &protection)) {
					if (*(BYTE*)0x39568d0 < 5) {
						*(BYTE*)0x3bd3dd5 = conditions[*(BYTE*)0x3bd3dd5];
						*(BYTE*)0x39568d0 = *(BYTE*)0x3bd3dd5;
						*(BYTE*)0x3957474 = *(BYTE*)0x3bd3dd5;
					}
					if (*(BYTE*)0x39568d1 < 5) {
						*(BYTE*)0x3bd4119 = conditions[*(BYTE*)0x3bd4119];
						*(BYTE*)0x39568d1 = *(BYTE*)0x3bd4119;
						*(BYTE*)0x3957475 = *(BYTE*)0x3bd4119;
					}
					if (*(BYTE*)0x39568d2 < 5) {
						*(BYTE*)0x3bd445d = conditions[*(BYTE*)0x3bd445d];
						*(BYTE*)0x39568d2 = *(BYTE*)0x3bd445d;
						*(BYTE*)0x3957476 = *(BYTE*)0x3bd445d;
					}
					if (*(BYTE*)0x39568d3 < 5) {
						*(BYTE*)0x3bd47a1 = conditions[*(BYTE*)0x3bd47a1];
						*(BYTE*)0x39568d3 = *(BYTE*)0x3bd47a1;
						*(BYTE*)0x3957477 = *(BYTE*)0x3bd47a1;
					}
					if (*(BYTE*)0x39568d4 < 5) {
						*(BYTE*)0x3bd4ae5 = conditions[*(BYTE*)0x3bd4ae5];
						*(BYTE*)0x39568d4 = *(BYTE*)0x3bd4ae5;
						*(BYTE*)0x3957478 = *(BYTE*)0x3bd4ae5;
					}
					if (*(BYTE*)0x39568d5 < 5) {
						*(BYTE*)0x3bd4e29 = conditions[*(BYTE*)0x3bd4e29];
						*(BYTE*)0x39568d5 = *(BYTE*)0x3bd4e29;
						*(BYTE*)0x3957479 = *(BYTE*)0x3bd4e29;
					}
					if (*(BYTE*)0x39568d6 < 5) {
						*(BYTE*)0x3bd516d = conditions[*(BYTE*)0x3bd516d];
						*(BYTE*)0x39568d6 = *(BYTE*)0x3bd516d;
						*(BYTE*)0x395747a = *(BYTE*)0x3bd516d;
					}
					if (*(BYTE*)0x39568d7 < 5) {
						*(BYTE*)0x3bd54b1 = conditions[*(BYTE*)0x3bd54b1];
						*(BYTE*)0x39568d7 = *(BYTE*)0x3bd54b1;
						*(BYTE*)0x395747b = *(BYTE*)0x3bd54b1;
					}
					if (*(BYTE*)0x39568d8 < 5) {
						*(BYTE*)0x3bd57f5 = conditions[*(BYTE*)0x3bd57f5];
						*(BYTE*)0x39568d8 = *(BYTE*)0x3bd57f5;
						*(BYTE*)0x395747c = *(BYTE*)0x3bd57f5;
					}
					if (*(BYTE*)0x39568d9 < 5) {
						*(BYTE*)0x3bd5b39 = conditions[*(BYTE*)0x3bd5b39];
						*(BYTE*)0x39568d9 = *(BYTE*)0x3bd5b39;
						*(BYTE*)0x395747d = *(BYTE*)0x3bd5b39;
					}
					if (*(BYTE*)0x39568da < 5) {
						*(BYTE*)0x3bd5e7d = conditions[*(BYTE*)0x3bd5e7d];
						*(BYTE*)0x39568da = *(BYTE*)0x3bd5e7d;
						*(BYTE*)0x395747e = *(BYTE*)0x3bd5e7d;
					}
					if (*(BYTE*)0x39568db < 5) {
						*(BYTE*)0x3bd61c1 = conditions[*(BYTE*)0x3bd61c1];
						*(BYTE*)0x39568db = *(BYTE*)0x3bd61c1;
						*(BYTE*)0x395747f = *(BYTE*)0x3bd61c1;
					}
					if (*(BYTE*)0x39568dc < 5) {
						*(BYTE*)0x3bd6505 = conditions[*(BYTE*)0x3bd6505];
						*(BYTE*)0x39568dc = *(BYTE*)0x3bd6505;
						*(BYTE*)0x3957480 = *(BYTE*)0x3bd6505;
					}
					if (*(BYTE*)0x39568dd < 5) {
						*(BYTE*)0x3bd6849 = conditions[*(BYTE*)0x3bd6849];
						*(BYTE*)0x39568dd = *(BYTE*)0x3bd6849;
						*(BYTE*)0x3957481 = *(BYTE*)0x3bd6849;
					}
					if (*(BYTE*)0x39568de < 5) {
						*(BYTE*)0x3bd6b8d = conditions[*(BYTE*)0x3bd6b8d];
						*(BYTE*)0x39568de = *(BYTE*)0x3bd6b8d;
						*(BYTE*)0x3957482 = *(BYTE*)0x3bd6b8d;
					}
					if (*(BYTE*)0x39568df < 5) {
						*(BYTE*)0x3bd6ed1 = conditions[*(BYTE*)0x3bd6ed1];
						*(BYTE*)0x39568df = *(BYTE*)0x3bd6ed1;
						*(BYTE*)0x3957483 = *(BYTE*)0x3bd6ed1;
					}
					if (*(BYTE*)0x39568e0 < 5) {
						*(BYTE*)0x3bd7215 = conditions[*(BYTE*)0x3bd7215];
						*(BYTE*)0x39568e0 = *(BYTE*)0x3bd7215;
						*(BYTE*)0x3957484 = *(BYTE*)0x3bd7215;
					}
					if (*(BYTE*)0x39568e1 < 5) {
						*(BYTE*)0x3bd7559 = conditions[*(BYTE*)0x3bd7559];
						*(BYTE*)0x39568e1 = *(BYTE*)0x3bd7559;
						*(BYTE*)0x3957485 = *(BYTE*)0x3bd7559;
					}
					if (*(BYTE*)0x39568e2 < 5) {
						*(BYTE*)0x3bd789d = conditions[*(BYTE*)0x3bd789d];
						*(BYTE*)0x39568e2 = *(BYTE*)0x3bd789d;
						*(BYTE*)0x3957486 = *(BYTE*)0x3bd789d;
					}
					if (*(BYTE*)0x39568e3 < 5) {
						*(BYTE*)0x3bd7be1 = conditions[*(BYTE*)0x3bd7be1];
						*(BYTE*)0x39568e3 = *(BYTE*)0x3bd7be1;
						*(BYTE*)0x3957487 = *(BYTE*)0x3bd7be1;
					}
					if (*(BYTE*)0x39568e4 < 5) {
						*(BYTE*)0x3bd7f25 = conditions[*(BYTE*)0x3bd7f25];
						*(BYTE*)0x39568e4 = *(BYTE*)0x3bd7f25;
						*(BYTE*)0x3957488 = *(BYTE*)0x3bd7f25;
					}
					if (*(BYTE*)0x39568e5 < 5) {
						*(BYTE*)0x3bd8269 = conditions[*(BYTE*)0x3bd8269];
						*(BYTE*)0x39568e5 = *(BYTE*)0x3bd8269;
						*(BYTE*)0x3957489 = *(BYTE*)0x3bd8269;
					}
					if (*(BYTE*)0x39568e6 < 5) {
						*(BYTE*)0x3bd85ad = conditions[*(BYTE*)0x3bd85ad];
						*(BYTE*)0x39568e6 = *(BYTE*)0x3bd85ad;
						*(BYTE*)0x395748a = *(BYTE*)0x3bd85ad;
					}
					if (*(BYTE*)0x39568e7 < 5) {
						*(BYTE*)0x3bd88f1 = conditions[*(BYTE*)0x3bd88f1];
						*(BYTE*)0x39568e7 = *(BYTE*)0x3bd88f1;
						*(BYTE*)0x395748b = *(BYTE*)0x3bd88f1;
					}
					if (*(BYTE*)0x39568e8 < 5) {
						*(BYTE*)0x3bd8c35 = conditions[*(BYTE*)0x3bd8c35];
						*(BYTE*)0x39568e8 = *(BYTE*)0x3bd8c35;
						*(BYTE*)0x395748c = *(BYTE*)0x3bd8c35;
					}
					if (*(BYTE*)0x39568e9 < 5) {
						*(BYTE*)0x3bd8f79 = conditions[*(BYTE*)0x3bd8f79];
						*(BYTE*)0x39568e9 = *(BYTE*)0x3bd8f79;
						*(BYTE*)0x395748d = *(BYTE*)0x3bd8f79;
					}
					if (*(BYTE*)0x39568ea < 5) {
						*(BYTE*)0x3bd92bd = conditions[*(BYTE*)0x3bd92bd];
						*(BYTE*)0x39568ea = *(BYTE*)0x3bd92bd;
						*(BYTE*)0x395748e = *(BYTE*)0x3bd92bd;
					}
					if (*(BYTE*)0x39568eb < 5) {
						*(BYTE*)0x3bd9601 = conditions[*(BYTE*)0x3bd9601];
						*(BYTE*)0x39568eb = *(BYTE*)0x3bd9601;
						*(BYTE*)0x395748f = *(BYTE*)0x3bd9601;
					}
					if (*(BYTE*)0x39568ec < 5) {
						*(BYTE*)0x3bd9945 = conditions[*(BYTE*)0x3bd9945];
						*(BYTE*)0x39568ec = *(BYTE*)0x3bd9945;
						*(BYTE*)0x3957490 = *(BYTE*)0x3bd9945;
					}
					if (*(BYTE*)0x39568ed < 5) {
						*(BYTE*)0x3bd9c89 = conditions[*(BYTE*)0x3bd9c89];
						*(BYTE*)0x39568ed = *(BYTE*)0x3bd9c89;
						*(BYTE*)0x3957491 = *(BYTE*)0x3bd9c89;
					}
					if (*(BYTE*)0x39568ee < 5) {
						*(BYTE*)0x3bd9fcd = conditions[*(BYTE*)0x3bd9fcd];
						*(BYTE*)0x39568ee = *(BYTE*)0x3bd9fcd;
						*(BYTE*)0x3957492 = *(BYTE*)0x3bd9fcd;
					}
					if (*(BYTE*)0x39568ef < 5) {
						*(BYTE*)0x3bda311 = conditions[*(BYTE*)0x3bda311];
						*(BYTE*)0x39568ef = *(BYTE*)0x3bda311;
						*(BYTE*)0x3957493 = *(BYTE*)0x3bda311;
					}
					if (*(BYTE*)0x395b524 < 5) {
						*(BYTE*)0x3bda655 = conditions[*(BYTE*)0x3bda655];
						*(BYTE*)0x395b524 = *(BYTE*)0x3bda655;
						*(BYTE*)0x395c0c8 = *(BYTE*)0x3bda655;
					}
					if (*(BYTE*)0x395b525 < 5) {
						*(BYTE*)0x3bda999 = conditions[*(BYTE*)0x3bda999];
						*(BYTE*)0x395b525 = *(BYTE*)0x3bda999;
						*(BYTE*)0x395c0c9 = *(BYTE*)0x3bda999;
					}
					if (*(BYTE*)0x395b526 < 5) {
						*(BYTE*)0x3bdacdd = conditions[*(BYTE*)0x3bdacdd];
						*(BYTE*)0x395b526 = *(BYTE*)0x3bdacdd;
						*(BYTE*)0x395c0ca = *(BYTE*)0x3bdacdd;
					}
					if (*(BYTE*)0x395b527 < 5) {
						*(BYTE*)0x3bdb021 = conditions[*(BYTE*)0x3bdb021];
						*(BYTE*)0x395b527 = *(BYTE*)0x3bdb021;
						*(BYTE*)0x395c0cb = *(BYTE*)0x3bdb021;
					}
					if (*(BYTE*)0x395b528 < 5) {
						*(BYTE*)0x3bdb365 = conditions[*(BYTE*)0x3bdb365];
						*(BYTE*)0x395b528 = *(BYTE*)0x3bdb365;
						*(BYTE*)0x395c0cc = *(BYTE*)0x3bdb365;
					}
					if (*(BYTE*)0x395b529 < 5) {
						*(BYTE*)0x3bdb6a9 = conditions[*(BYTE*)0x3bdb6a9];
						*(BYTE*)0x395b529 = *(BYTE*)0x3bdb6a9;
						*(BYTE*)0x395c0cd = *(BYTE*)0x3bdb6a9;
					}
					if (*(BYTE*)0x395b52a < 5) {
						*(BYTE*)0x3bdb9ed = conditions[*(BYTE*)0x3bdb9ed];
						*(BYTE*)0x395b52a = *(BYTE*)0x3bdb9ed;
						*(BYTE*)0x395c0ce = *(BYTE*)0x3bdb9ed;
					}
					if (*(BYTE*)0x395b52b < 5) {
						*(BYTE*)0x3bdbd31 = conditions[*(BYTE*)0x3bdbd31];
						*(BYTE*)0x395b52b = *(BYTE*)0x3bdbd31;
						*(BYTE*)0x395c0cf = *(BYTE*)0x3bdbd31;
					}
					if (*(BYTE*)0x395b52c < 5) {
						*(BYTE*)0x3bdc075 = conditions[*(BYTE*)0x3bdc075];
						*(BYTE*)0x395b52c = *(BYTE*)0x3bdc075;
						*(BYTE*)0x395c0d0 = *(BYTE*)0x3bdc075;
					}
					if (*(BYTE*)0x395b52d < 5) {
						*(BYTE*)0x3bdc3b9 = conditions[*(BYTE*)0x3bdc3b9];
						*(BYTE*)0x395b52d = *(BYTE*)0x3bdc3b9;
						*(BYTE*)0x395c0d1 = *(BYTE*)0x3bdc3b9;
					}
					if (*(BYTE*)0x395b52e < 5) {
						*(BYTE*)0x3bdc6fd = conditions[*(BYTE*)0x3bdc6fd];
						*(BYTE*)0x395b52e = *(BYTE*)0x3bdc6fd;
						*(BYTE*)0x395c0d2 = *(BYTE*)0x3bdc6fd;
					}
					if (*(BYTE*)0x395b52f < 5) {
						*(BYTE*)0x3bdca41 = conditions[*(BYTE*)0x3bdca41];
						*(BYTE*)0x395b52f = *(BYTE*)0x3bdca41;
						*(BYTE*)0x395c0d3 = *(BYTE*)0x3bdca41;
					}
					if (*(BYTE*)0x395b530 < 5) {
						*(BYTE*)0x3bdcd85 = conditions[*(BYTE*)0x3bdcd85];
						*(BYTE*)0x395b530 = *(BYTE*)0x3bdcd85;
						*(BYTE*)0x395c0d4 = *(BYTE*)0x3bdcd85;
					}
					if (*(BYTE*)0x395b531 < 5) {
						*(BYTE*)0x3bdd0c9 = conditions[*(BYTE*)0x3bdd0c9];
						*(BYTE*)0x395b531 = *(BYTE*)0x3bdd0c9;
						*(BYTE*)0x395c0d5 = *(BYTE*)0x3bdd0c9;
					}
					if (*(BYTE*)0x395b532 < 5) {
						*(BYTE*)0x3bdd40d = conditions[*(BYTE*)0x3bdd40d];
						*(BYTE*)0x395b532 = *(BYTE*)0x3bdd40d;
						*(BYTE*)0x395c0d6 = *(BYTE*)0x3bdd40d;
					}
					if (*(BYTE*)0x395b533 < 5) {
						*(BYTE*)0x3bdd751 = conditions[*(BYTE*)0x3bdd751];
						*(BYTE*)0x395b533 = *(BYTE*)0x3bdd751;
						*(BYTE*)0x395c0d7 = *(BYTE*)0x3bdd751;
					}
					if (*(BYTE*)0x395b534 < 5) {
						*(BYTE*)0x3bdda95 = conditions[*(BYTE*)0x3bdda95];
						*(BYTE*)0x395b534 = *(BYTE*)0x3bdda95;
						*(BYTE*)0x395c0d8 = *(BYTE*)0x3bdda95;
					}
					if (*(BYTE*)0x395b535 < 5) {
						*(BYTE*)0x3bdddd9 = conditions[*(BYTE*)0x3bdddd9];
						*(BYTE*)0x395b535 = *(BYTE*)0x3bdddd9;
						*(BYTE*)0x395c0d9 = *(BYTE*)0x3bdddd9;
					}
					if (*(BYTE*)0x395b536 < 5) {
						*(BYTE*)0x3bde11d = conditions[*(BYTE*)0x3bde11d];
						*(BYTE*)0x395b536 = *(BYTE*)0x3bde11d;
						*(BYTE*)0x395c0da = *(BYTE*)0x3bde11d;
					}
					if (*(BYTE*)0x395b537 < 5) {
						*(BYTE*)0x3bde461 = conditions[*(BYTE*)0x3bde461];
						*(BYTE*)0x395b537 = *(BYTE*)0x3bde461;
						*(BYTE*)0x395c0db = *(BYTE*)0x3bde461;
					}
					if (*(BYTE*)0x395b538 < 5) {
						*(BYTE*)0x3bde7a5 = conditions[*(BYTE*)0x3bde7a5];
						*(BYTE*)0x395b538 = *(BYTE*)0x3bde7a5;
						*(BYTE*)0x395c0dc = *(BYTE*)0x3bde7a5;
					}
					if (*(BYTE*)0x395b539 < 5) {
						*(BYTE*)0x3bdeae9 = conditions[*(BYTE*)0x3bdeae9];
						*(BYTE*)0x395b539 = *(BYTE*)0x3bdeae9;
						*(BYTE*)0x395c0dd = *(BYTE*)0x3bdeae9;
					}
					if (*(BYTE*)0x395b53a < 5) {
						*(BYTE*)0x3bdee2d = conditions[*(BYTE*)0x3bdee2d];
						*(BYTE*)0x395b53a = *(BYTE*)0x3bdee2d;
						*(BYTE*)0x395c0de = *(BYTE*)0x3bdee2d;
					}
					if (*(BYTE*)0x395b53b < 5) {
						*(BYTE*)0x3bdf171 = conditions[*(BYTE*)0x3bdf171];
						*(BYTE*)0x395b53b = *(BYTE*)0x3bdf171;
						*(BYTE*)0x395c0df = *(BYTE*)0x3bdf171;
					}
					if (*(BYTE*)0x395b53c < 5) {
						*(BYTE*)0x3bdf4b5 = conditions[*(BYTE*)0x3bdf4b5];
						*(BYTE*)0x395b53c = *(BYTE*)0x3bdf4b5;
						*(BYTE*)0x395c0e0 = *(BYTE*)0x3bdf4b5;
					}
					if (*(BYTE*)0x395b53d < 5) {
						*(BYTE*)0x3bdf7f9 = conditions[*(BYTE*)0x3bdf7f9];
						*(BYTE*)0x395b53d = *(BYTE*)0x3bdf7f9;
						*(BYTE*)0x395c0e1 = *(BYTE*)0x3bdf7f9;
					}
					if (*(BYTE*)0x395b53e < 5) {
						*(BYTE*)0x3bdfb3d = conditions[*(BYTE*)0x3bdfb3d];
						*(BYTE*)0x395b53e = *(BYTE*)0x3bdfb3d;
						*(BYTE*)0x395c0e2 = *(BYTE*)0x3bdfb3d;
					}
					if (*(BYTE*)0x395b53f < 5) {
						*(BYTE*)0x3bdfe81 = conditions[*(BYTE*)0x3bdfe81];
						*(BYTE*)0x395b53f = *(BYTE*)0x3bdfe81;
						*(BYTE*)0x395c0e3 = *(BYTE*)0x3bdfe81;
					}
					if (*(BYTE*)0x395b540 < 5) {
						*(BYTE*)0x3be01c5 = conditions[*(BYTE*)0x3be01c5];
						*(BYTE*)0x395b540 = *(BYTE*)0x3be01c5;
						*(BYTE*)0x395c0e4 = *(BYTE*)0x3be01c5;
					}
					if (*(BYTE*)0x395b541 < 5) {
						*(BYTE*)0x3be0509 = conditions[*(BYTE*)0x3be0509];
						*(BYTE*)0x395b541 = *(BYTE*)0x3be0509;
						*(BYTE*)0x395c0e5 = *(BYTE*)0x3be0509;
					}
					if (*(BYTE*)0x395b542 < 5) {
						*(BYTE*)0x3be084d = conditions[*(BYTE*)0x3be084d];
						*(BYTE*)0x395b542 = *(BYTE*)0x3be084d;
						*(BYTE*)0x395c0e6 = *(BYTE*)0x3be084d;
					}
					if (*(BYTE*)0x395b543 < 5) {
						*(BYTE*)0x3be0b91 = conditions[*(BYTE*)0x3be0b91];
						*(BYTE*)0x395b543 = *(BYTE*)0x3be0b91;
						*(BYTE*)0x395c0e7 = *(BYTE*)0x3be0b91;
					}
					Log(&k_gameplay, "Condition arrows tweaked.");
				}
				else {
					Log(&k_gameplay, "Problem tweaking condition arrows.");
				}
				counter1++;
			}
			break;
		case gvWE9LEPC:
			/**
			* 0x3ad6434 // presence in out-of-match menus?: 0=no, 1=yes
			*/
			if (gameplay_config.tweakedConditionArrowsEnabled && *(BYTE*)0x38b3178 < 255 && counter1 == 0 && *(BYTE*)0x3ad6434 == 0) {
				if (VirtualProtect((LPVOID)0x38b3178, sizeof(0x38b3178), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3179, sizeof(0x38b3179), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b317a, sizeof(0x38b317a), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b317b, sizeof(0x38b317b), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b317c, sizeof(0x38b317c), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b317d, sizeof(0x38b317d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b317e, sizeof(0x38b317e), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b317f, sizeof(0x38b317f), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3180, sizeof(0x38b3180), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3181, sizeof(0x38b3181), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3182, sizeof(0x38b3182), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3183, sizeof(0x38b3183), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3184, sizeof(0x38b3184), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3185, sizeof(0x38b3185), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3186, sizeof(0x38b3186), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3187, sizeof(0x38b3187), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3188, sizeof(0x38b3188), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3189, sizeof(0x38b3189), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b318a, sizeof(0x38b318a), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b318b, sizeof(0x38b318b), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b318c, sizeof(0x38b318c), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b318d, sizeof(0x38b318d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b318e, sizeof(0x38b318e), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b318f, sizeof(0x38b318f), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3190, sizeof(0x38b3190), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3191, sizeof(0x38b3191), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3192, sizeof(0x38b3192), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3193, sizeof(0x38b3193), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3194, sizeof(0x38b3194), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3195, sizeof(0x38b3195), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3196, sizeof(0x38b3196), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3197, sizeof(0x38b3197), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d1c, sizeof(0x38b3d1c), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d1d, sizeof(0x38b3d1d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d1e, sizeof(0x38b3d1e), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d1f, sizeof(0x38b3d1f), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d20, sizeof(0x38b3d20), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d21, sizeof(0x38b3d21), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d22, sizeof(0x38b3d22), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d23, sizeof(0x38b3d23), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d24, sizeof(0x38b3d24), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d25, sizeof(0x38b3d25), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d26, sizeof(0x38b3d26), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d27, sizeof(0x38b3d27), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d28, sizeof(0x38b3d28), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d29, sizeof(0x38b3d29), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d2a, sizeof(0x38b3d2a), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d2b, sizeof(0x38b3d2b), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d2c, sizeof(0x38b3d2c), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d2d, sizeof(0x38b3d2d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d2e, sizeof(0x38b3d2e), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d2f, sizeof(0x38b3d2f), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d30, sizeof(0x38b3d30), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d31, sizeof(0x38b3d31), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d32, sizeof(0x38b3d32), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d33, sizeof(0x38b3d33), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d34, sizeof(0x38b3d34), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d35, sizeof(0x38b3d35), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d36, sizeof(0x38b3d36), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d37, sizeof(0x38b3d37), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d38, sizeof(0x38b3d38), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d39, sizeof(0x38b3d39), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d3a, sizeof(0x38b3d3a), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b3d3b, sizeof(0x38b3d3b), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dcc, sizeof(0x38b7dcc), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dcd, sizeof(0x38b7dcd), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dce, sizeof(0x38b7dce), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dcf, sizeof(0x38b7dcf), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dd0, sizeof(0x38b7dd0), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dd1, sizeof(0x38b7dd1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dd2, sizeof(0x38b7dd2), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dd3, sizeof(0x38b7dd3), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dd4, sizeof(0x38b7dd4), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dd5, sizeof(0x38b7dd5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dd6, sizeof(0x38b7dd6), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dd7, sizeof(0x38b7dd7), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dd8, sizeof(0x38b7dd8), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dd9, sizeof(0x38b7dd9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dda, sizeof(0x38b7dda), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7ddb, sizeof(0x38b7ddb), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7ddc, sizeof(0x38b7ddc), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7ddd, sizeof(0x38b7ddd), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dde, sizeof(0x38b7dde), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7ddf, sizeof(0x38b7ddf), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7de0, sizeof(0x38b7de0), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7de1, sizeof(0x38b7de1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7de2, sizeof(0x38b7de2), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7de3, sizeof(0x38b7de3), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7de4, sizeof(0x38b7de4), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7de5, sizeof(0x38b7de5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7de6, sizeof(0x38b7de6), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7de7, sizeof(0x38b7de7), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7de8, sizeof(0x38b7de8), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7de9, sizeof(0x38b7de9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7dea, sizeof(0x38b7dea), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b7deb, sizeof(0x38b7deb), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8970, sizeof(0x38b8970), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8971, sizeof(0x38b8971), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8972, sizeof(0x38b8972), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8973, sizeof(0x38b8973), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8974, sizeof(0x38b8974), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8975, sizeof(0x38b8975), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8976, sizeof(0x38b8976), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8977, sizeof(0x38b8977), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8978, sizeof(0x38b8978), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8979, sizeof(0x38b8979), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b897a, sizeof(0x38b897a), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b897b, sizeof(0x38b897b), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b897c, sizeof(0x38b897c), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b897d, sizeof(0x38b897d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b897e, sizeof(0x38b897e), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b897f, sizeof(0x38b897f), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8980, sizeof(0x38b8980), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8981, sizeof(0x38b8981), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8982, sizeof(0x38b8982), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8983, sizeof(0x38b8983), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8984, sizeof(0x38b8984), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8985, sizeof(0x38b8985), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8986, sizeof(0x38b8986), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8987, sizeof(0x38b8987), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8988, sizeof(0x38b8988), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b8989, sizeof(0x38b8989), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b898a, sizeof(0x38b898a), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b898b, sizeof(0x38b898b), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b898c, sizeof(0x38b898c), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b898d, sizeof(0x38b898d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b898e, sizeof(0x38b898e), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x38b898f, sizeof(0x38b898f), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5b915, sizeof(0x3b5b915), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5bc59, sizeof(0x3b5bc59), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5bf9d, sizeof(0x3b5bf9d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5c2e1, sizeof(0x3b5c2e1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5c625, sizeof(0x3b5c625), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5c969, sizeof(0x3b5c969), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5ccad, sizeof(0x3b5ccad), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5cff1, sizeof(0x3b5cff1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5d335, sizeof(0x3b5d335), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5d679, sizeof(0x3b5d679), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5d9bd, sizeof(0x3b5d9bd), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5dd01, sizeof(0x3b5dd01), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5e045, sizeof(0x3b5e045), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5e389, sizeof(0x3b5e389), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5e6cd, sizeof(0x3b5e6cd), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5ea11, sizeof(0x3b5ea11), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5ed55, sizeof(0x3b5ed55), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5f099, sizeof(0x3b5f099), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5f3dd, sizeof(0x3b5f3dd), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5f721, sizeof(0x3b5f721), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5fa65, sizeof(0x3b5fa65), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b5fda9, sizeof(0x3b5fda9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b600ed, sizeof(0x3b600ed), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b60431, sizeof(0x3b60431), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b60775, sizeof(0x3b60775), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b60ab9, sizeof(0x3b60ab9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b60dfd, sizeof(0x3b60dfd), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b61141, sizeof(0x3b61141), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b61485, sizeof(0x3b61485), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b617c9, sizeof(0x3b617c9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b61b0d, sizeof(0x3b61b0d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b61e51, sizeof(0x3b61e51), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b62195, sizeof(0x3b62195), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b624d9, sizeof(0x3b624d9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b6281d, sizeof(0x3b6281d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b62b61, sizeof(0x3b62b61), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b62ea5, sizeof(0x3b62ea5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b631e9, sizeof(0x3b631e9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b6352d, sizeof(0x3b6352d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b63871, sizeof(0x3b63871), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b63bb5, sizeof(0x3b63bb5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b63ef9, sizeof(0x3b63ef9), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b6423d, sizeof(0x3b6423d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b64581, sizeof(0x3b64581), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b648c5, sizeof(0x3b648c5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b64c09, sizeof(0x3b64c09), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b64f4d, sizeof(0x3b64f4d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b65291, sizeof(0x3b65291), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b655d5, sizeof(0x3b655d5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b65919, sizeof(0x3b65919), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b65c5d, sizeof(0x3b65c5d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b65fa1, sizeof(0x3b65fa1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b662e5, sizeof(0x3b662e5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b66629, sizeof(0x3b66629), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b6696d, sizeof(0x3b6696d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b66cb1, sizeof(0x3b66cb1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b66ff5, sizeof(0x3b66ff5), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b67339, sizeof(0x3b67339), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b6767d, sizeof(0x3b6767d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b679c1, sizeof(0x3b679c1), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b67d05, sizeof(0x3b67d05), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b68049, sizeof(0x3b68049), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b6838d, sizeof(0x3b6838d), newProtection, &protection) && 
					VirtualProtect((LPVOID)0x3b686d1, sizeof(0x3b686d1), newProtection, &protection)) {
					if (*(BYTE*)0x38b3178 < 5) {
						*(BYTE*)0x3b5b915 = conditions[*(BYTE*)0x3b5b915];
						*(BYTE*)0x38b3178 = *(BYTE*)0x3b5b915;
						*(BYTE*)0x38b3d1c = *(BYTE*)0x3b5b915;
					}
					if (*(BYTE*)0x38b3179 < 5) {
						*(BYTE*)0x3b5bc59 = conditions[*(BYTE*)0x3b5bc59];
						*(BYTE*)0x38b3179 = *(BYTE*)0x3b5bc59;
						*(BYTE*)0x38b3d1d = *(BYTE*)0x3b5bc59;
					}
					if (*(BYTE*)0x38b317a < 5) {
						*(BYTE*)0x3b5bf9d = conditions[*(BYTE*)0x3b5bf9d];
						*(BYTE*)0x38b317a = *(BYTE*)0x3b5bf9d;
						*(BYTE*)0x38b3d1e = *(BYTE*)0x3b5bf9d;
					}
					if (*(BYTE*)0x38b317b < 5) {
						*(BYTE*)0x3b5c2e1 = conditions[*(BYTE*)0x3b5c2e1];
						*(BYTE*)0x38b317b = *(BYTE*)0x3b5c2e1;
						*(BYTE*)0x38b3d1f = *(BYTE*)0x3b5c2e1;
					}
					if (*(BYTE*)0x38b317c < 5) {
						*(BYTE*)0x3b5c625 = conditions[*(BYTE*)0x3b5c625];
						*(BYTE*)0x38b317c = *(BYTE*)0x3b5c625;
						*(BYTE*)0x38b3d20 = *(BYTE*)0x3b5c625;
					}
					if (*(BYTE*)0x38b317d < 5) {
						*(BYTE*)0x3b5c969 = conditions[*(BYTE*)0x3b5c969];
						*(BYTE*)0x38b317d = *(BYTE*)0x3b5c969;
						*(BYTE*)0x38b3d21 = *(BYTE*)0x3b5c969;
					}
					if (*(BYTE*)0x38b317e < 5) {
						*(BYTE*)0x3b5ccad = conditions[*(BYTE*)0x3b5ccad];
						*(BYTE*)0x38b317e = *(BYTE*)0x3b5ccad;
						*(BYTE*)0x38b3d22 = *(BYTE*)0x3b5ccad;
					}
					if (*(BYTE*)0x38b317f < 5) {
						*(BYTE*)0x3b5cff1 = conditions[*(BYTE*)0x3b5cff1];
						*(BYTE*)0x38b317f = *(BYTE*)0x3b5cff1;
						*(BYTE*)0x38b3d23 = *(BYTE*)0x3b5cff1;
					}
					if (*(BYTE*)0x38b3180 < 5) {
						*(BYTE*)0x3b5d335 = conditions[*(BYTE*)0x3b5d335];
						*(BYTE*)0x38b3180 = *(BYTE*)0x3b5d335;
						*(BYTE*)0x38b3d24 = *(BYTE*)0x3b5d335;
					}
					if (*(BYTE*)0x38b3181 < 5) {
						*(BYTE*)0x3b5d679 = conditions[*(BYTE*)0x3b5d679];
						*(BYTE*)0x38b3181 = *(BYTE*)0x3b5d679;
						*(BYTE*)0x38b3d25 = *(BYTE*)0x3b5d679;
					}
					if (*(BYTE*)0x38b3182 < 5) {
						*(BYTE*)0x3b5d9bd = conditions[*(BYTE*)0x3b5d9bd];
						*(BYTE*)0x38b3182 = *(BYTE*)0x3b5d9bd;
						*(BYTE*)0x38b3d26 = *(BYTE*)0x3b5d9bd;
					}
					if (*(BYTE*)0x38b3183 < 5) {
						*(BYTE*)0x3b5dd01 = conditions[*(BYTE*)0x3b5dd01];
						*(BYTE*)0x38b3183 = *(BYTE*)0x3b5dd01;
						*(BYTE*)0x38b3d27 = *(BYTE*)0x3b5dd01;
					}
					if (*(BYTE*)0x38b3184 < 5) {
						*(BYTE*)0x3b5e045 = conditions[*(BYTE*)0x3b5e045];
						*(BYTE*)0x38b3184 = *(BYTE*)0x3b5e045;
						*(BYTE*)0x38b3d28 = *(BYTE*)0x3b5e045;
					}
					if (*(BYTE*)0x38b3185 < 5) {
						*(BYTE*)0x3b5e389 = conditions[*(BYTE*)0x3b5e389];
						*(BYTE*)0x38b3185 = *(BYTE*)0x3b5e389;
						*(BYTE*)0x38b3d29 = *(BYTE*)0x3b5e389;
					}
					if (*(BYTE*)0x38b3186 < 5) {
						*(BYTE*)0x3b5e6cd = conditions[*(BYTE*)0x3b5e6cd];
						*(BYTE*)0x38b3186 = *(BYTE*)0x3b5e6cd;
						*(BYTE*)0x38b3d2a = *(BYTE*)0x3b5e6cd;
					}
					if (*(BYTE*)0x38b3187 < 5) {
						*(BYTE*)0x3b5ea11 = conditions[*(BYTE*)0x3b5ea11];
						*(BYTE*)0x38b3187 = *(BYTE*)0x3b5ea11;
						*(BYTE*)0x38b3d2b = *(BYTE*)0x3b5ea11;
					}
					if (*(BYTE*)0x38b3188 < 5) {
						*(BYTE*)0x3b5ed55 = conditions[*(BYTE*)0x3b5ed55];
						*(BYTE*)0x38b3188 = *(BYTE*)0x3b5ed55;
						*(BYTE*)0x38b3d2c = *(BYTE*)0x3b5ed55;
					}
					if (*(BYTE*)0x38b3189 < 5) {
						*(BYTE*)0x3b5f099 = conditions[*(BYTE*)0x3b5f099];
						*(BYTE*)0x38b3189 = *(BYTE*)0x3b5f099;
						*(BYTE*)0x38b3d2d = *(BYTE*)0x3b5f099;
					}
					if (*(BYTE*)0x38b318a < 5) {
						*(BYTE*)0x3b5f3dd = conditions[*(BYTE*)0x3b5f3dd];
						*(BYTE*)0x38b318a = *(BYTE*)0x3b5f3dd;
						*(BYTE*)0x38b3d2e = *(BYTE*)0x3b5f3dd;
					}
					if (*(BYTE*)0x38b318b < 5) {
						*(BYTE*)0x3b5f721 = conditions[*(BYTE*)0x3b5f721];
						*(BYTE*)0x38b318b = *(BYTE*)0x3b5f721;
						*(BYTE*)0x38b3d2f = *(BYTE*)0x3b5f721;
					}
					if (*(BYTE*)0x38b318c < 5) {
						*(BYTE*)0x3b5fa65 = conditions[*(BYTE*)0x3b5fa65];
						*(BYTE*)0x38b318c = *(BYTE*)0x3b5fa65;
						*(BYTE*)0x38b3d30 = *(BYTE*)0x3b5fa65;
					}
					if (*(BYTE*)0x38b318d < 5) {
						*(BYTE*)0x3b5fda9 = conditions[*(BYTE*)0x3b5fda9];
						*(BYTE*)0x38b318d = *(BYTE*)0x3b5fda9;
						*(BYTE*)0x38b3d31 = *(BYTE*)0x3b5fda9;
					}
					if (*(BYTE*)0x38b318e < 5) {
						*(BYTE*)0x3b600ed = conditions[*(BYTE*)0x3b600ed];
						*(BYTE*)0x38b318e = *(BYTE*)0x3b600ed;
						*(BYTE*)0x38b3d32 = *(BYTE*)0x3b600ed;
					}
					if (*(BYTE*)0x38b318f < 5) {
						*(BYTE*)0x3b60431 = conditions[*(BYTE*)0x3b60431];
						*(BYTE*)0x38b318f = *(BYTE*)0x3b60431;
						*(BYTE*)0x38b3d33 = *(BYTE*)0x3b60431;
					}
					if (*(BYTE*)0x38b3190 < 5) {
						*(BYTE*)0x3b60775 = conditions[*(BYTE*)0x3b60775];
						*(BYTE*)0x38b3190 = *(BYTE*)0x3b60775;
						*(BYTE*)0x38b3d34 = *(BYTE*)0x3b60775;
					}
					if (*(BYTE*)0x38b3191 < 5) {
						*(BYTE*)0x3b60ab9 = conditions[*(BYTE*)0x3b60ab9];
						*(BYTE*)0x38b3191 = *(BYTE*)0x3b60ab9;
						*(BYTE*)0x38b3d35 = *(BYTE*)0x3b60ab9;
					}
					if (*(BYTE*)0x38b3192 < 5) {
						*(BYTE*)0x3b60dfd = conditions[*(BYTE*)0x3b60dfd];
						*(BYTE*)0x38b3192 = *(BYTE*)0x3b60dfd;
						*(BYTE*)0x38b3d36 = *(BYTE*)0x3b60dfd;
					}
					if (*(BYTE*)0x38b3193 < 5) {
						*(BYTE*)0x3b61141 = conditions[*(BYTE*)0x3b61141];
						*(BYTE*)0x38b3193 = *(BYTE*)0x3b61141;
						*(BYTE*)0x38b3d37 = *(BYTE*)0x3b61141;
					}
					if (*(BYTE*)0x38b3194 < 5) {
						*(BYTE*)0x3b61485 = conditions[*(BYTE*)0x3b61485];
						*(BYTE*)0x38b3194 = *(BYTE*)0x3b61485;
						*(BYTE*)0x38b3d38 = *(BYTE*)0x3b61485;
					}
					if (*(BYTE*)0x38b3195 < 5) {
						*(BYTE*)0x3b617c9 = conditions[*(BYTE*)0x3b617c9];
						*(BYTE*)0x38b3195 = *(BYTE*)0x3b617c9;
						*(BYTE*)0x38b3d39 = *(BYTE*)0x3b617c9;
					}
					if (*(BYTE*)0x38b3196 < 5) {
						*(BYTE*)0x3b61b0d = conditions[*(BYTE*)0x3b61b0d];
						*(BYTE*)0x38b3196 = *(BYTE*)0x3b61b0d;
						*(BYTE*)0x38b3d3a = *(BYTE*)0x3b61b0d;
					}
					if (*(BYTE*)0x38b3197 < 5) {
						*(BYTE*)0x3b61e51 = conditions[*(BYTE*)0x3b61e51];
						*(BYTE*)0x38b3197 = *(BYTE*)0x3b61e51;
						*(BYTE*)0x38b3d3b = *(BYTE*)0x3b61e51;
					}
					if (*(BYTE*)0x38b7dcc < 5) {
						*(BYTE*)0x3b62195 = conditions[*(BYTE*)0x3b62195];
						*(BYTE*)0x38b7dcc = *(BYTE*)0x3b62195;
						*(BYTE*)0x38b8970 = *(BYTE*)0x3b62195;
					}
					if (*(BYTE*)0x38b7dcd < 5) {
						*(BYTE*)0x3b624d9 = conditions[*(BYTE*)0x3b624d9];
						*(BYTE*)0x38b7dcd = *(BYTE*)0x3b624d9;
						*(BYTE*)0x38b8971 = *(BYTE*)0x3b624d9;
					}
					if (*(BYTE*)0x38b7dce < 5) {
						*(BYTE*)0x3b6281d = conditions[*(BYTE*)0x3b6281d];
						*(BYTE*)0x38b7dce = *(BYTE*)0x3b6281d;
						*(BYTE*)0x38b8972 = *(BYTE*)0x3b6281d;
					}
					if (*(BYTE*)0x38b7dcf < 5) {
						*(BYTE*)0x3b62b61 = conditions[*(BYTE*)0x3b62b61];
						*(BYTE*)0x38b7dcf = *(BYTE*)0x3b62b61;
						*(BYTE*)0x38b8973 = *(BYTE*)0x3b62b61;
					}
					if (*(BYTE*)0x38b7dd0 < 5) {
						*(BYTE*)0x3b62ea5 = conditions[*(BYTE*)0x3b62ea5];
						*(BYTE*)0x38b7dd0 = *(BYTE*)0x3b62ea5;
						*(BYTE*)0x38b8974 = *(BYTE*)0x3b62ea5;
					}
					if (*(BYTE*)0x38b7dd1 < 5) {
						*(BYTE*)0x3b631e9 = conditions[*(BYTE*)0x3b631e9];
						*(BYTE*)0x38b7dd1 = *(BYTE*)0x3b631e9;
						*(BYTE*)0x38b8975 = *(BYTE*)0x3b631e9;
					}
					if (*(BYTE*)0x38b7dd2 < 5) {
						*(BYTE*)0x3b6352d = conditions[*(BYTE*)0x3b6352d];
						*(BYTE*)0x38b7dd2 = *(BYTE*)0x3b6352d;
						*(BYTE*)0x38b8976 = *(BYTE*)0x3b6352d;
					}
					if (*(BYTE*)0x38b7dd3 < 5) {
						*(BYTE*)0x3b63871 = conditions[*(BYTE*)0x3b63871];
						*(BYTE*)0x38b7dd3 = *(BYTE*)0x3b63871;
						*(BYTE*)0x38b8977 = *(BYTE*)0x3b63871;
					}
					if (*(BYTE*)0x38b7dd4 < 5) {
						*(BYTE*)0x3b63bb5 = conditions[*(BYTE*)0x3b63bb5];
						*(BYTE*)0x38b7dd4 = *(BYTE*)0x3b63bb5;
						*(BYTE*)0x38b8978 = *(BYTE*)0x3b63bb5;
					}
					if (*(BYTE*)0x38b7dd5 < 5) {
						*(BYTE*)0x3b63ef9 = conditions[*(BYTE*)0x3b63ef9];
						*(BYTE*)0x38b7dd5 = *(BYTE*)0x3b63ef9;
						*(BYTE*)0x38b8979 = *(BYTE*)0x3b63ef9;
					}
					if (*(BYTE*)0x38b7dd6 < 5) {
						*(BYTE*)0x3b6423d = conditions[*(BYTE*)0x3b6423d];
						*(BYTE*)0x38b7dd6 = *(BYTE*)0x3b6423d;
						*(BYTE*)0x38b897a = *(BYTE*)0x3b6423d;
					}
					if (*(BYTE*)0x38b7dd7 < 5) {
						*(BYTE*)0x3b64581 = conditions[*(BYTE*)0x3b64581];
						*(BYTE*)0x38b7dd7 = *(BYTE*)0x3b64581;
						*(BYTE*)0x38b897b = *(BYTE*)0x3b64581;
					}
					if (*(BYTE*)0x38b7dd8 < 5) {
						*(BYTE*)0x3b648c5 = conditions[*(BYTE*)0x3b648c5];
						*(BYTE*)0x38b7dd8 = *(BYTE*)0x3b648c5;
						*(BYTE*)0x38b897c = *(BYTE*)0x3b648c5;
					}
					if (*(BYTE*)0x38b7dd9 < 5) {
						*(BYTE*)0x3b64c09 = conditions[*(BYTE*)0x3b64c09];
						*(BYTE*)0x38b7dd9 = *(BYTE*)0x3b64c09;
						*(BYTE*)0x38b897d = *(BYTE*)0x3b64c09;
					}
					if (*(BYTE*)0x38b7dda < 5) {
						*(BYTE*)0x3b64f4d = conditions[*(BYTE*)0x3b64f4d];
						*(BYTE*)0x38b7dda = *(BYTE*)0x3b64f4d;
						*(BYTE*)0x38b897e = *(BYTE*)0x3b64f4d;
					}
					if (*(BYTE*)0x38b7ddb < 5) {
						*(BYTE*)0x3b65291 = conditions[*(BYTE*)0x3b65291];
						*(BYTE*)0x38b7ddb = *(BYTE*)0x3b65291;
						*(BYTE*)0x38b897f = *(BYTE*)0x3b65291;
					}
					if (*(BYTE*)0x38b7ddc < 5) {
						*(BYTE*)0x3b655d5 = conditions[*(BYTE*)0x3b655d5];
						*(BYTE*)0x38b7ddc = *(BYTE*)0x3b655d5;
						*(BYTE*)0x38b8980 = *(BYTE*)0x3b655d5;
					}
					if (*(BYTE*)0x38b7ddd < 5) {
						*(BYTE*)0x3b65919 = conditions[*(BYTE*)0x3b65919];
						*(BYTE*)0x38b7ddd = *(BYTE*)0x3b65919;
						*(BYTE*)0x38b8981 = *(BYTE*)0x3b65919;
					}
					if (*(BYTE*)0x38b7dde < 5) {
						*(BYTE*)0x3b65c5d = conditions[*(BYTE*)0x3b65c5d];
						*(BYTE*)0x38b7dde = *(BYTE*)0x3b65c5d;
						*(BYTE*)0x38b8982 = *(BYTE*)0x3b65c5d;
					}
					if (*(BYTE*)0x38b7ddf < 5) {
						*(BYTE*)0x3b65fa1 = conditions[*(BYTE*)0x3b65fa1];
						*(BYTE*)0x38b7ddf = *(BYTE*)0x3b65fa1;
						*(BYTE*)0x38b8983 = *(BYTE*)0x3b65fa1;
					}
					if (*(BYTE*)0x38b7de0 < 5) {
						*(BYTE*)0x3b662e5 = conditions[*(BYTE*)0x3b662e5];
						*(BYTE*)0x38b7de0 = *(BYTE*)0x3b662e5;
						*(BYTE*)0x38b8984 = *(BYTE*)0x3b662e5;
					}
					if (*(BYTE*)0x38b7de1 < 5) {
						*(BYTE*)0x3b66629 = conditions[*(BYTE*)0x3b66629];
						*(BYTE*)0x38b7de1 = *(BYTE*)0x3b66629;
						*(BYTE*)0x38b8985 = *(BYTE*)0x3b66629;
					}
					if (*(BYTE*)0x38b7de2 < 5) {
						*(BYTE*)0x3b6696d = conditions[*(BYTE*)0x3b6696d];
						*(BYTE*)0x38b7de2 = *(BYTE*)0x3b6696d;
						*(BYTE*)0x38b8986 = *(BYTE*)0x3b6696d;
					}
					if (*(BYTE*)0x38b7de3 < 5) {
						*(BYTE*)0x3b66cb1 = conditions[*(BYTE*)0x3b66cb1];
						*(BYTE*)0x38b7de3 = *(BYTE*)0x3b66cb1;
						*(BYTE*)0x38b8987 = *(BYTE*)0x3b66cb1;
					}
					if (*(BYTE*)0x38b7de4 < 5) {
						*(BYTE*)0x3b66ff5 = conditions[*(BYTE*)0x3b66ff5];
						*(BYTE*)0x38b7de4 = *(BYTE*)0x3b66ff5;
						*(BYTE*)0x38b8988 = *(BYTE*)0x3b66ff5;
					}
					if (*(BYTE*)0x38b7de5 < 5) {
						*(BYTE*)0x3b67339 = conditions[*(BYTE*)0x3b67339];
						*(BYTE*)0x38b7de5 = *(BYTE*)0x3b67339;
						*(BYTE*)0x38b8989 = *(BYTE*)0x3b67339;
					}
					if (*(BYTE*)0x38b7de6 < 5) {
						*(BYTE*)0x3b6767d = conditions[*(BYTE*)0x3b6767d];
						*(BYTE*)0x38b7de6 = *(BYTE*)0x3b6767d;
						*(BYTE*)0x38b898a = *(BYTE*)0x3b6767d;
					}
					if (*(BYTE*)0x38b7de7 < 5) {
						*(BYTE*)0x3b679c1 = conditions[*(BYTE*)0x3b679c1];
						*(BYTE*)0x38b7de7 = *(BYTE*)0x3b679c1;
						*(BYTE*)0x38b898b = *(BYTE*)0x3b679c1;
					}
					if (*(BYTE*)0x38b7de8 < 5) {
						*(BYTE*)0x3b67d05 = conditions[*(BYTE*)0x3b67d05];
						*(BYTE*)0x38b7de8 = *(BYTE*)0x3b67d05;
						*(BYTE*)0x38b898c = *(BYTE*)0x3b67d05;
					}
					if (*(BYTE*)0x38b7de9 < 5) {
						*(BYTE*)0x3b68049 = conditions[*(BYTE*)0x3b68049];
						*(BYTE*)0x38b7de9 = *(BYTE*)0x3b68049;
						*(BYTE*)0x38b898d = *(BYTE*)0x3b68049;
					}
					if (*(BYTE*)0x38b7dea < 5) {
						*(BYTE*)0x3b6838d = conditions[*(BYTE*)0x3b6838d];
						*(BYTE*)0x38b7dea = *(BYTE*)0x3b6838d;
						*(BYTE*)0x38b898e = *(BYTE*)0x3b6838d;
					}
					if (*(BYTE*)0x38b7deb < 5) {
						*(BYTE*)0x3b686d1 = conditions[*(BYTE*)0x3b686d1];
						*(BYTE*)0x38b7deb = *(BYTE*)0x3b686d1;
						*(BYTE*)0x38b898f = *(BYTE*)0x3b686d1;
					}
					Log(&k_gameplay, "Condition arrows tweaked.");
				}
				else {
					Log(&k_gameplay, "Problem tweaking condition arrows.");
				}
				counter1++;
			}
			break;
	}
	return false;
}

bool SetAdditionalSubstitutionData()
{
	switch (GetPESInfo()->GameVersion) {
		case gvPES5PC:
			/**
			* 0xfdcc1b // full time: 0=no, 1=yes
			* 0x3966b7c // half: 0=1st, 1=2nd, 2=1Ex., 3=2Ex., 4=P.K.
			* 0x3966dcc // half/full time/play again menu: 0=hidden, 1=visible, 2=select team/injury discovered, 5=strip selection
			* 0x3be0f53 // max. no. of substitutions
			*/
			if (gameplay_config.additionalSubExtraTimeEnabled && counter2 == 0 && *(BYTE*)0xfdcc1b == 0 && *(BYTE*)0x3966b7c == 1 && 
				*(BYTE*)0x3966dcc == 1) {
					if (VirtualProtect((LPVOID)0x3be0f53, sizeof(0x3be0f53), newProtection, &protection)) {
						LogWithNumber(&k_gameplay, "Max. no. of substitutions was: %d", *(BYTE*)0x3be0f53);
						*(BYTE*)0x3be0f53 += 1;
						LogWithNumber(&k_gameplay, "Max. no. of substitutions now: %d", *(BYTE*)0x3be0f53);
					}
					else {
						Log(&k_gameplay, "Problem changing additional substitution in extra time.");
					}
					counter2++;
			}
			else if (gameplay_config.additionalSubExtraTimeEnabled && counter2 > 0 && ((*(BYTE*)0xfdcc1b == 0 && *(BYTE*)0x3966dcc == 5) || 
				(*(BYTE*)0xfdcc1b == 0 && *(BYTE*)0x3966b7c == 4) || *(BYTE*)0xfdcc1b == 1)) {
					if (VirtualProtect((LPVOID)0x3be0f53, sizeof(0x3be0f53), newProtection, &protection)) {
						LogWithNumber(&k_gameplay, "Max. no. of substitutions was: %d", *(BYTE*)0x3be0f53);
						*(BYTE*)0x3be0f53 -= 1;
						LogWithNumber(&k_gameplay, "Max. no. of substitutions now: %d", *(BYTE*)0x3be0f53);
					}
					else {
						Log(&k_gameplay, "Problem changing additional substitution in extra time.");
					}
					counter2 = 0;
			}
			break;
		case gvWE9LEPC:
			/**
			* 0xf16bb3 // full time: 0=no, 1=yes
			* 0x38c3424 // half: 0=1st, 1=2nd, 2=1Ex., 3=2Ex., 4=P.K.
			* 0x38c3674 // half/full time/play again menu: 0=hidden, 1=visible, 2=select team/injury discovered, 5=strip selection
			* 0x3b68a93 // max. no. of substitutions
			*/
			if (gameplay_config.additionalSubExtraTimeEnabled && counter2 == 0 && *(BYTE*)0xf16bb3 == 0 && *(BYTE*)0x38c3424 == 1 && 
				*(BYTE*)0x38c3674 == 1) {
					if (VirtualProtect((LPVOID)0x3b68a93, sizeof(0x3b68a93), newProtection, &protection)) {
						LogWithNumber(&k_gameplay, "Max. no. of substitutions was: %d", *(BYTE*)0x3b68a93);
						*(BYTE*)0x3b68a93 += 1;
						LogWithNumber(&k_gameplay, "Max. no. of substitutions now: %d", *(BYTE*)0x3b68a93);
					}
					else {
						Log(&k_gameplay, "Problem changing additional substitution in extra time.");
					}
					counter2++;
			}
			else if (gameplay_config.additionalSubExtraTimeEnabled && counter2 > 0 && ((*(BYTE*)0xf16bb3 == 0 && *(BYTE*)0x38c3674 == 5) || 
				(*(BYTE*)0xf16bb3 == 0 && *(BYTE*)0x38c3424 == 4) || *(BYTE*)0xf16bb3 == 1)) {
					if (VirtualProtect((LPVOID)0x3b68a93, sizeof(0x3b68a93), newProtection, &protection)) {
						LogWithNumber(&k_gameplay, "Max. no. of substitutions was: %d", *(BYTE*)0x3b68a93);
						*(BYTE*)0x3b68a93 -= 1;
						LogWithNumber(&k_gameplay, "Max. no. of substitutions now: %d", *(BYTE*)0x3b68a93);
					}
					else {
						Log(&k_gameplay, "Problem changing additional substitution in extra time.");
					}
					counter2 = 0;
			}
			break;
	}
	return false;
}

/**
 * Returns true if successful.
 */
bool readConfig(config_t& config)
{
    string cfgFile(GetPESInfo()->mydir);
    cfgFile += "\\gameplay.cfg";

	FILE* cfg = fopen(cfgFile.c_str(), "rt");
	if (cfg == NULL) return false;

	char str[BUFLEN];
	char name[BUFLEN];
	int value = 0;

	char *pName = NULL, *pValue = NULL, *comment = NULL;
	while (true)
	{
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);
		if (feof(cfg)) break;

		// skip comments
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';

		// parse the line
		pName = pValue = NULL;
		ZeroMemory(name, BUFLEN); value = 0;
		char* eq = strstr(str, "=");
		if (eq == NULL || eq[1] == '\0') continue;

		eq[0] = '\0';
		pName = str; pValue = eq + 1;

		ZeroMemory(name, NULL); 
		sscanf(pName, "%s", name);

		if (strcmp(name, "auto-ball-physics.enabled")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_gameplay, "ReadConfig: auto-ball-physics.enabled = (%d)", value);
			config.autoBallPhysicsEnabled = (value > 0);
		}
		else if (strcmp(name, "random-fine-ball-physics.enabled")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_gameplay, "ReadConfig: random-fine-ball-physics.enabled = (%d)", value);
			config.randomFineBallPhysicsEnabled = (value > 0);
		}
		else if (strcmp(name, "hard-pitch.summer.probability")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_gameplay, "ReadConfig: hard-pitch.summer.probability = (%d)", value);
			config.hardpitch_summer_probability = value;
		}
		else if (strcmp(name, "hard-pitch.winter.probability")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_gameplay, "ReadConfig: hard-pitch.winter.probability = (%d)", value);
			config.hardpitch_winter_probability = value;
		}
		else if (strcmp(name, "tweaked-condition-arrows.enabled")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_gameplay, "ReadConfig: tweaked-condition-arrows.enabled = (%d)", value);
			config.tweakedConditionArrowsEnabled = (value > 0);
		}
		else if (strcmp(name, "excellent-condition.replacement")==0)
		{
			string value(pValue);
			string_strip(value);
			config.excellentcondition_replacement = value;
		}
		else if (strcmp(name, "good-condition.replacement")==0)
		{
			string value(pValue);
			string_strip(value);
			config.goodcondition_replacement = value;
		}
		else if (strcmp(name, "normal-condition.replacement")==0)
		{
			string value(pValue);
			string_strip(value);
			config.normalcondition_replacement = value;
		}
		else if (strcmp(name, "poor-condition.replacement")==0)
		{
			string value(pValue);
			string_strip(value);
			config.poorcondition_replacement = value;
		}
		else if (strcmp(name, "terrible-condition.replacement")==0)
		{
			string value(pValue);
			string_strip(value);
			config.terriblecondition_replacement = value;
		}
		else if (strcmp(name, "additional-sub-extra-time.enabled")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_gameplay, "ReadConfig: additional-sub-extra-time.enabled = (%d)", value);
			config.additionalSubExtraTimeEnabled = (value > 0);
		}
		else if (strcmp(name, "ball-player.proximity")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball-player.proximity = (%g)", (double)value);
			config.ballplayer_proximity = value;
		}
		else if (strcmp(name, "ball.gravity")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.gravity = (%g)", (double)value);
			config.ball_gravity = value;
		}
		else if (strcmp(name, "ball.global-bounce-stick")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.global-bounce-stick = (%g)", (double)value);
			config.ball_globalbouncestick = value;
		}
		else if (strcmp(name, "ball.spin-termination-speed")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.spin-termination-speed = (%g)", (double)value);
			config.ball_spinterminationspeed = value;
		}
		else if (strcmp(name, "ball.bounce-stick-frequency")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.bounce-stick-frequency = (%g)", (double)value);
			config.ball_bouncestickfrequency = value;
		}
		else if (strcmp(name, "ball.topspin")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.topspin = (%g)", (double)value);
			config.ball_topspin = value;
		}
		else if (strcmp(name, "ball.air-resistance")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.air-resistance = (%g)", (double)value);
			config.ball_airresistance = value;
		}
		else if (strcmp(name, "ball.touch-move-rate")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.touch-move-rate = (%g)", (double)value);
			config.ball_touchmoverate = value;
		}
		else if (strcmp(name, "ball.retardation-rate")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.retardation-rate = (%g)", (double)value);
			config.ball_retardationrate = value;
		}
		else if (strcmp(name, "player.anim-speed")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: player.anim-speed = (%g)", (double)value);
			config.player_animspeed = value;
		}
		else if (strcmp(name, "ball-player.proximity.hard")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball-player.proximity.hard = (%g)", (double)value);
			config.ballplayer_proximity_hard = value;
		}
		else if (strcmp(name, "ball.gravity.hard")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.gravity.hard = (%g)", (double)value);
			config.ball_gravity_hard = value;
		}
		else if (strcmp(name, "ball.global-bounce-stick.hard")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.global-bounce-stick.hard = (%g)", (double)value);
			config.ball_globalbouncestick_hard = value;
		}
		else if (strcmp(name, "ball.spin-termination-speed.hard")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.spin-termination-speed.hard = (%g)", (double)value);
			config.ball_spinterminationspeed_hard = value;
		}
		else if (strcmp(name, "ball.bounce-stick-frequency.hard")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.bounce-stick-frequency.hard = (%g)", (double)value);
			config.ball_bouncestickfrequency_hard = value;
		}
		else if (strcmp(name, "ball.topspin.hard")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.topspin.hard = (%g)", (double)value);
			config.ball_topspin_hard = value;
		}
		else if (strcmp(name, "ball.air-resistance.hard")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.air-resistance.hard = (%g)", (double)value);
			config.ball_airresistance_hard = value;
		}
		else if (strcmp(name, "ball.touch-move-rate.hard")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.touch-move-rate.hard = (%g)", (double)value);
			config.ball_touchmoverate_hard = value;
		}
		else if (strcmp(name, "ball.retardation-rate.hard")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.retardation-rate.hard = (%g)", (double)value);
			config.ball_retardationrate_hard = value;
		}
		else if (strcmp(name, "player.anim-speed.hard")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: player.anim-speed.hard = (%g)", (double)value);
			config.player_animspeed_hard = value;
		}
		else if (strcmp(name, "ball-player.proximity.rain")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball-player.proximity.rain = (%g)", (double)value);
			config.ballplayer_proximity_rain = value;
		}
		else if (strcmp(name, "ball.gravity.rain")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.gravity.rain = (%g)", (double)value);
			config.ball_gravity_rain = value;
		}
		else if (strcmp(name, "ball.global-bounce-stick.rain")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.global-bounce-stick.rain = (%g)", (double)value);
			config.ball_globalbouncestick_rain = value;
		}
		else if (strcmp(name, "ball.spin-termination-speed.rain")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.spin-termination-speed.rain = (%g)", (double)value);
			config.ball_spinterminationspeed_rain = value;
		}
		else if (strcmp(name, "ball.bounce-stick-frequency.rain")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.bounce-stick-frequency.rain = (%g)", (double)value);
			config.ball_bouncestickfrequency_rain = value;
		}
		else if (strcmp(name, "ball.topspin.rain")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.topspin.rain = (%g)", (double)value);
			config.ball_topspin_rain = value;
		}
		else if (strcmp(name, "ball.air-resistance.rain")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.air-resistance.rain = (%g)", (double)value);
			config.ball_airresistance_rain = value;
		}
		else if (strcmp(name, "ball.touch-move-rate.rain")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.touch-move-rate.rain = (%g)", (double)value);
			config.ball_touchmoverate_rain = value;
		}
		else if (strcmp(name, "ball.retardation-rate.rain")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.retardation-rate.rain = (%g)", (double)value);
			config.ball_retardationrate_rain = value;
		}
		else if (strcmp(name, "player.anim-speed.rain")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: player.anim-speed.rain = (%g)", (double)value);
			config.player_animspeed_rain = value;
		}
		else if (strcmp(name, "ball-player.proximity.snow")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball-player.proximity.snow = (%g)", (double)value);
			config.ballplayer_proximity_snow = value;
		}
		else if (strcmp(name, "ball.gravity.snow")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.gravity.snow = (%g)", (double)value);
			config.ball_gravity_snow = value;
		}
		else if (strcmp(name, "ball.global-bounce-stick.snow")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.global-bounce-stick.snow = (%g)", (double)value);
			config.ball_globalbouncestick_snow = value;
		}
		else if (strcmp(name, "ball.spin-termination-speed.snow")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.spin-termination-speed.snow = (%g)", (double)value);
			config.ball_spinterminationspeed_snow = value;
		}
		else if (strcmp(name, "ball.bounce-stick-frequency.snow")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.bounce-stick-frequency.snow = (%g)", (double)value);
			config.ball_bouncestickfrequency_snow = value;
		}
		else if (strcmp(name, "ball.topspin.snow")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.topspin.snow = (%g)", (double)value);
			config.ball_topspin_snow = value;
		}
		else if (strcmp(name, "ball.air-resistance.snow")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.air-resistance.snow = (%g)", (double)value);
			config.ball_airresistance_snow = value;
		}
		else if (strcmp(name, "ball.touch-move-rate.snow")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.touch-move-rate.snow = (%g)", (double)value);
			config.ball_touchmoverate_snow = value;
		}
		else if (strcmp(name, "ball.retardation-rate.snow")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: ball.retardation-rate.snow = (%g)", (double)value);
			config.ball_retardationrate_snow = value;
		}
		else if (strcmp(name, "player.anim-speed.snow")==0)
		{
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_gameplay, "ReadConfig: player.anim-speed.snow = (%g)", (double)value);
			config.player_animspeed_snow = value;
		}
	}
	fclose(cfg);
	return true;
}