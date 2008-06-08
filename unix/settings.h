#ifndef __SETTINGS__
#define __SETTINGS__

typedef struct _SETTINGS
{
  int loopcount;
  int fadeseconds;
  int fadedelayseconds;
} SETTINGS,*PSETTINGS,*LPSETTINGS;

void DefaultSettings(LPSETTINGS pSettings);
int LoadSettings(LPSETTINGS pSettings);
int SaveSettings(LPSETTINGS pSettings);


#endif
