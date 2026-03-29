#ifndef _PLAY_CONFIG_H_
#define _PLAY_CONFIG_H_

#include "../streamfile.h"
#include "../vgmstream.h"


typedef struct {
    int allow_play_forever;
    int disable_config_override;

    /* song mofidiers */
    int play_forever;           /* keeps looping forever (needs loop points) */
    int ignore_loop;            /* ignores loops points */
    int force_loop;             /* enables full loops (0..samples) if file doesn't have loop points */
    int really_force_loop;      /* forces full loops even if file has loop points */
    int ignore_fade;            /*  don't fade after N loops */

    /* song processing */
    double loop_count;          /* target loops */
    double fade_delay;          /* fade delay after target loops */
    double fade_time;           /* fade period after target loops */

  //int downmix;                /* max number of channels allowed (0=disable downmix) */

} vgmstream_cfg_t;

void vgmstream_apply_config(VGMSTREAM* vgmstream, vgmstream_cfg_t* pcfg);

#endif
