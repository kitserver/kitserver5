// referees.cpp
#include <windows.h>
#include <stdio.h>
#include "referees.h"
#include "kload_exp.h"

KMOD k_referees={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;

DWORD protection;
DWORD newProtection = PAGE_EXECUTE_READWRITE;
int strictness;

unsigned long rand64()
{
    static unsigned long seed;
    seed = seed * 6364136223846793005 + 1442695040888963407;
    return seed;
}

class config_t 
{
public:
    config_t() : additionalRefsExhibitionEnabled(false), cardStrictnessEnabled(false), randomCardStrictnessEnabled(false), 
		cardstrictness_random_minimum(0), cardstrictness_random_maximum(0), card_strictness(0) {}
	bool additionalRefsExhibitionEnabled;
	bool cardStrictnessEnabled;
	bool randomCardStrictnessEnabled;
	int cardstrictness_random_minimum;
	int cardstrictness_random_maximum;
	int card_strictness;
};

config_t referees_config;

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void InitReferees();
bool SetAdditionalRefereesData();
void SetRefereesData1();
void SetRefereesData2();
bool readConfig(config_t& config);

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_referees,"Attaching dll...");

		switch (GetPESInfo()->GameVersion) {
			case gvPES5PC: //support for PES5 PC...
            case gvWE9LEPC: //... and WE9:LE PC
				break;
            default:
                Log(&k_referees,"Your game version is currently not supported!");
                return false;
		}
		
		hInst=hInstance;

		RegisterKModule(&k_referees);
		
		HookFunction(hk_D3D_Create,(DWORD)InitReferees);
		HookFunction(hk_AfterReadFile,(DWORD)SetAdditionalRefereesData);
		HookFunction(hk_UniSplit,(DWORD)SetRefereesData1);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_referees,"Detaching dll...");

		UnhookFunction(hk_AfterReadFile,(DWORD)SetRefereesData2);
		UnhookFunction(hk_AfterReadFile,(DWORD)SetAdditionalRefereesData);
		UnhookFunction(hk_UniSplit,(DWORD)SetRefereesData1);
		Log(&k_referees,"Detaching done.");
	}
	
	return true;
}

void InitReferees()
{
	UnhookFunction(hk_D3D_Create,(DWORD)InitReferees);

	// read configuration
    readConfig(referees_config);

	Log(&k_referees, "Module initialized.");
}

bool SetAdditionalRefereesData()
{
	switch (GetPESInfo()->GameVersion) {
		case gvPES5PC:
			if (referees_config.additionalRefsExhibitionEnabled) {
				if (VirtualProtect((LPVOID)0xfe0d74, sizeof(0xfe0d74), newProtection, &protection)) {
					*(BYTE*)0xfe0d74 = 22;
					Log(&k_referees,"Additional referees in exhibition mode added.");
				}
				else {
					Log(&k_referees, "Problem adding additional referees in exhibition mode.");
				}
			}
			break;
		case gvWE9LEPC:
			if (referees_config.additionalRefsExhibitionEnabled) {
				if (VirtualProtect((LPVOID)0xf1ad04, sizeof(0xf1ad04), newProtection, &protection)) {
					*(BYTE*)0xf1ad04 = 22;
					Log(&k_referees,"Additional referees in exhibition mode added.");
				}
				else {
					Log(&k_referees, "Problem adding additional referees in exhibition mode.");
				}
			}
			break;
	}
	return false;
}

void SetRefereesData1()
{	
	if (referees_config.cardStrictnessEnabled && referees_config.randomCardStrictnessEnabled) {
		srand(GetTickCount());
		strictness = rand64() % (referees_config.cardstrictness_random_maximum + 1 - referees_config.cardstrictness_random_minimum) + referees_config.cardstrictness_random_minimum;
	}
	else if (referees_config.cardStrictnessEnabled && !referees_config.randomCardStrictnessEnabled) {
		strictness = referees_config.card_strictness;
	}

	HookFunction(hk_AfterReadFile,(DWORD)SetRefereesData2);
}


void SetRefereesData2()
{
	switch (GetPESInfo()->GameVersion) {
		case gvPES5PC:
			if (referees_config.cardStrictnessEnabled) {
				if (VirtualProtect((LPVOID)0xff6bec, sizeof(0xff6bec), newProtection, &protection) && 
					VirtualProtect((LPVOID)0xff6bf0, sizeof(0xff6bf0), newProtection, &protection) && 
					VirtualProtect((LPVOID)0xff6bfc, sizeof(0xff6bfc), newProtection, &protection)) {
						*(int*)0xff6bec = strictness;
						*(int*)0xff6bf0 = strictness;
						*(int*)0xff6bfc = strictness;
						LogWithNumber(&k_referees, "Card strictness: %d", *(int*)0xff6bec);
						LogWithNumber(&k_referees, "Card strictness: %d", *(int*)0xff6bf0);
						LogWithNumber(&k_referees, "Card strictness: %d", *(int*)0xff6bfc);
				}
				else {
					Log(&k_referees, "Problem changing card strictness.");
				}
			}
			break;
		case gvWE9LEPC:
			if (referees_config.cardStrictnessEnabled) {
				if (VirtualProtect((LPVOID)0xf7768c, sizeof(0xf7768c), newProtection, &protection) && 
					VirtualProtect((LPVOID)0xf77690, sizeof(0xf77690), newProtection, &protection) && 
					VirtualProtect((LPVOID)0xf7769c, sizeof(0xf7769c), newProtection, &protection)) {
						*(int*)0xf7768c = strictness;
						*(int*)0xf77690 = strictness;
						*(int*)0xf7769c = strictness;
						LogWithNumber(&k_referees, "Card strictness: %d", *(int*)0xf7768c);
						LogWithNumber(&k_referees, "Card strictness: %d", *(int*)0xf77690);
						LogWithNumber(&k_referees, "Card strictness: %d", *(int*)0xf7769c);
				}
				else {
					Log(&k_referees, "Problem changing card strictness.");
				}
			}
			break;
	}
}

/**
 * Returns true if successful.
 */
bool readConfig(config_t& config)
{
    string cfgFile(GetPESInfo()->mydir);
    cfgFile += "\\referees.cfg";

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

		if (strcmp(name, "additional-refs-exhibition.enabled")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_referees, "ReadConfig: additional-refs-exhibition.enabled = (%d)", value);
			config.additionalRefsExhibitionEnabled = (value > 0);
		}
		else if (strcmp(name, "card-strictness.enabled")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_referees, "ReadConfig: card-strictness.enabled = (%d)", value);
			config.cardStrictnessEnabled = (value > 0);
		}
		else if (strcmp(name, "random-card-strictness.enabled")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_referees, "ReadConfig: random-card-strictness.enabled = (%d)", value);
			config.randomCardStrictnessEnabled = (value > 0);
		}
		else if (strcmp(name, "card-strictness.random.minimum")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_referees, "ReadConfig: card-strictness.random.minimum = (%d)", value);
			config.cardstrictness_random_minimum = value;
		}
		else if (strcmp(name, "card-strictness.random.maximum")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_referees, "ReadConfig: card-strictness.random.maximum = (%d)", value);
			config.cardstrictness_random_maximum = value;
		}
		else if (strcmp(name, "card.strictness")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_referees, "ReadConfig: card.strictness = (%d)", value);
			config.card_strictness = value;
		}
	}
	fclose(cfg);
	return true;
}