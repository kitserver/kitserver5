// fserv_config.cpp

#include <stdio.h>
#include <string.h>

#include "fserv_config.h"
#include "kload_exp.h"

extern KMOD k_fserv;

/**
 * Returns true if successful.
 */
BOOL ReadConfig(FSERV_CONFIG* config, char* cfgFile)
{
	if (config == NULL) return false;

	FILE* cfg = fopen(cfgFile, "rt");
	if (cfg == NULL) return false;

	char str[BUFLEN];
	char name[BUFLEN];
	int value = 0;
	float dvalue = 0.0f;

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

		if (lstrcmp(name, "dump.textures")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LOG(&k_fserv,"ReadConfig: dump.textures = (%d)", value);
			config->dump_face_textures = (value > 0);
		}
       	else if (lstrcmp(name, "hd.face.width")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LOG(&k_fserv,"ReadConfig: hd.face.width = (%d)", value);
            if (value < MIN_HD_FACE_WIDTH) {
                value = MIN_HD_FACE_WIDTH;
                LOG(&k_fserv,"Enforcing minimum for hd.face.width: %d", value);
            }
			config->hd_face_width = value;
		}
       	else if (lstrcmp(name, "hd.face.height")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LOG(&k_fserv,"ReadConfig: hd.face.height = (%d)", value);
            if (value < MIN_HD_FACE_HEIGHT) {
                value = MIN_HD_FACE_HEIGHT;
                LOG(&k_fserv,"Enforcing minimum for hd.face.height: %d", value);
            }
			config->hd_face_height = value;
		}
	}
	fclose(cfg);

	return true;
}

