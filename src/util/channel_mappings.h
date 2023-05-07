#ifndef _CHANNEL_MAPPING_H
#define _CHANNEL_MAPPING_H

/* standard WAVEFORMATEXTENSIBLE speaker positions */
typedef enum {
    speaker_FL  = (1 << 0),     /* front left */
    speaker_FR  = (1 << 1),     /* front right */
    speaker_FC  = (1 << 2),     /* front center */
    speaker_LFE = (1 << 3),     /* low frequency effects */
    speaker_BL  = (1 << 4),     /* back left */
    speaker_BR  = (1 << 5),     /* back right */
    speaker_FLC = (1 << 6),     /* front left center */
    speaker_FRC = (1 << 7),     /* front right center */
    speaker_BC  = (1 << 8),     /* back center */
    speaker_SL  = (1 << 9),     /* side left */
    speaker_SR  = (1 << 10),    /* side right */

    speaker_TC  = (1 << 11),    /* top center*/
    speaker_TFL = (1 << 12),    /* top front left */
    speaker_TFC = (1 << 13),    /* top front center */
    speaker_TFR = (1 << 14),    /* top front right */
    speaker_TBL = (1 << 15),    /* top back left */
    speaker_TBC = (1 << 16),    /* top back center */
    speaker_TBR = (1 << 17),    /* top back left */

} speaker_t;

/* typical mappings that metas may use to set channel_layout (but plugin must actually use it)
 * (in order, so 3ch file could be mapped to FL FR FC or FL FR LFE but not LFE FL FR)
 * not too sure about names but no clear standards */
typedef enum {
    mapping_MONO             = speaker_FC,
    mapping_STEREO           = speaker_FL | speaker_FR,
    mapping_2POINT1          = speaker_FL | speaker_FR | speaker_LFE,
    mapping_2POINT1_xiph     = speaker_FL | speaker_FR | speaker_FC, /* aka 3STEREO? */
    mapping_QUAD             = speaker_FL | speaker_FR | speaker_BL  | speaker_BR,
    mapping_QUAD_surround    = speaker_FL | speaker_FR | speaker_FC  | speaker_BC,
    mapping_QUAD_side        = speaker_FL | speaker_FR | speaker_SL  | speaker_SR,
    mapping_5POINT0          = speaker_FL | speaker_FR | speaker_LFE | speaker_BL | speaker_BR,
    mapping_5POINT0_xiph     = speaker_FL | speaker_FR | speaker_FC  | speaker_BL | speaker_BR,
    mapping_5POINT0_surround = speaker_FL | speaker_FR | speaker_FC  | speaker_SL | speaker_SR,
    mapping_5POINT1          = speaker_FL | speaker_FR | speaker_FC  | speaker_LFE | speaker_BL | speaker_BR,
    mapping_5POINT1_surround = speaker_FL | speaker_FR | speaker_FC  | speaker_LFE | speaker_SL | speaker_SR,
    mapping_7POINT0          = speaker_FL | speaker_FR | speaker_FC  | speaker_LFE | speaker_BC | speaker_FLC | speaker_FRC,
    mapping_7POINT1          = speaker_FL | speaker_FR | speaker_FC  | speaker_LFE | speaker_BL | speaker_BR  | speaker_FLC | speaker_FRC,
    mapping_7POINT1_surround = speaker_FL | speaker_FR | speaker_FC  | speaker_LFE | speaker_BL | speaker_BR  | speaker_SL  | speaker_SR,
} channel_mapping_t;

#endif
