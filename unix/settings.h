#ifndef __SETTINGS__
#define __SETTINGS__

typedef struct
{
    bool loop_forever;
	int loop_count;
	double fade_length;
	double fade_delay;
} Settings;

extern Settings vgmstream_cfg;

void debugMessage(const char *str);
void vgmstream_cfg_load();
void vgmstream_cfg_save();

#endif
