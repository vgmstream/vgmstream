#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"


static bool is_oor(STREAMFILE* sf);

/* .OOR ("OptimizedObsforR") - rUGP/AGES engine audio [Muv-Luv (multi), Liberation Maiden SIN (PS3/Vita)] */
VGMSTREAM* init_vgmstream_oor(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;


    /* checks */
    if (read_u8(0x00, sf) != 0x08 && read_u32be(0x00, sf) != 0x48000000) //v0 and v1 first page headers
        return NULL;
    if (!check_extensions(sf, "oor"))
        return NULL;

    // bitpacked header, should fail during on init with bad data but do minor validations to skip some allocs
    if (!is_oor(sf))
        return NULL;

#ifdef VGM_USE_VORBIS
    vorbis_custom_codec_data* data = NULL;
    vorbis_custom_config cfg = {0}; //loads info on success

    data = init_vorbis_custom(sf, 0x00, VORBIS_OOR, &cfg);
    if (!data) return NULL;

    start_offset = cfg.data_start_offset;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(cfg.channels, 0);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_OOR;
    vgmstream->sample_rate = cfg.sample_rate;
    vgmstream->num_samples = cfg.last_granule;

    vgmstream->layout_type = layout_none;
    vgmstream->coding_type = coding_VORBIS_custom;
    vgmstream->codec_data = data;
    data = NULL;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;

    //TODO: improve
    // v0 files don't set last granule (must be done after opening streamfiles)
    if (!cfg.last_granule)
        vgmstream->num_samples = vorbis_custom_get_samples(vgmstream);

    return vgmstream;
fail:
    free_vorbis_custom(data);
    close_vgmstream(vgmstream);
    return NULL;
#else
    return NULL;
#endif
}

// OOR is bitpacked but try to determine if bytes look like a .oor (will fail later if we picked a wrong candidate).
static bool is_oor(STREAMFILE* sf) {
    static uint8_t empty_granule[0x09];
    uint8_t data[0x10] = {0};

    read_streamfile(data, 0x00, sizeof(data), sf);
    
    int page_version;
    int head_pos;
    if (data[0x00] == 0x48 && memcmp(data + 0x01, empty_granule, 9) == 0) {
        // V1: bits 01 0010 00 + granule + header
        head_pos = 0x0A;
        page_version = 1;
    }
    else if (data[0x00] == 0x08) {
        // V0: bits 00 0010 00 + header
        head_pos = 0x01;
        page_version = 0;
    }
    else {
        return false;
    }
    
    uint16_t head = (data[head_pos] << 8) | data[head_pos+1];
    int version     = (head >> 14) & 0x03; //2b
    int channels    = (head >> 11) & 0x07; //3b
    int sr_selector = (head >> 9) & 0x03; //3b
    int srate       = (head >> 1) & 0xFF; //3b

    if (version != page_version || channels == 0)
        return false;

    if (sr_selector == 3 && srate > 10)
        return false;

    return true;
}
