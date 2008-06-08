#include "settings.h"
#include <audacious/plugin.h>

#define CUBE_CONFIG_TAG "vgmstream"

static ConfigDb *GetConfigFile( void )
{
  ConfigDb *cfg = aud_cfg_db_open();
  return cfg;
}

void DefaultSettings(LPSETTINGS pSettings)
{
  memset(pSettings,0,sizeof(SETTINGS));
  
  pSettings->loopcount = 1;
  pSettings->fadeseconds = 0;
  pSettings->fadedelayseconds = 0;
}

int LoadSettings(LPSETTINGS pSettings)
{
  ConfigDb *cfg = GetConfigFile();
  if (!cfg)
  {
    DefaultSettings(pSettings);
  }
  else
  {  
    int bRet = (aud_cfg_db_get_int(cfg,CUBE_CONFIG_TAG,"loopcount",&pSettings->loopcount) && 
		aud_cfg_db_get_int(cfg,CUBE_CONFIG_TAG,"fadeseconds",&pSettings->fadeseconds) &&
		aud_cfg_db_get_int(cfg,CUBE_CONFIG_TAG,"fadedelayseconds",&pSettings->fadedelayseconds));
    
    aud_cfg_db_close(cfg); 
    
    // check if reading one value failed.  If so, then use defaults
    if (!bRet)
      DefaultSettings(pSettings);
  }
  return 1;
}

int SaveSettings(LPSETTINGS pSettings)
{
  ConfigDb *cfg = GetConfigFile();
  if (!cfg)
    return 0;

  aud_cfg_db_set_int(cfg,CUBE_CONFIG_TAG,"loopcount",pSettings->loopcount);
  aud_cfg_db_set_int(cfg,CUBE_CONFIG_TAG,"fadeseconds",pSettings->fadeseconds);
  aud_cfg_db_set_int(cfg,CUBE_CONFIG_TAG,"fadedelayseconds",pSettings->fadedelayseconds);

  aud_cfg_db_close(cfg);
  return 1;
}
