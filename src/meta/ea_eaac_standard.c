#include "meta.h"
#include "../util/endianness.h"


#define EAAC_BLOCKID1_HEADER            0x48 /* 'H' */


/* .SNR+SNS - from EA latest games (~2005-2010), v0 header */
VGMSTREAM* init_vgmstream_ea_snr_sns(STREAMFILE* sf) {
    eaac_meta_t info = {0};

    /* checks */
    if (!check_extensions(sf,"snr"))
        return NULL;

    info.sf_head = sf;
    info.head_offset = 0x00;
    info.body_offset = 0x00;
    info.type = meta_EA_SNR_SNS;
    info.standalone = true;
    return load_vgmstream_ea_eaac(&info);
}

/* .SPS - from EA latest games (~2010~present), v1 header */
VGMSTREAM* init_vgmstream_ea_sps(STREAMFILE* sf) {
    eaac_meta_t info = {0};

    /* checks */
    if (read_u8(0x00, sf) != EAAC_BLOCKID1_HEADER) /* validated later but fails faster */
        return NULL;
    if (!check_extensions(sf,"sps"))
        return NULL;

    info.sf_head = sf;
    info.head_offset = 0x00;
    info.type = meta_EA_SPS;
    info.standalone = true;
    return load_vgmstream_ea_eaac(&info);
}

/* .SNU - from EA Redwood Shores/Visceral games (Dead Space, Dante's Inferno, The Godfather 2), v0 header */
VGMSTREAM* init_vgmstream_ea_snu(STREAMFILE* sf) {
    eaac_meta_t info = {0};
    read_u32_t read_u32 = NULL;

    /* checks */
    if (!check_extensions(sf,"snu"))
        return NULL;

    /* EA SNU header (BE/LE depending on platform) */
    /* 0x00(1): related to sample rate? (03=48000)
     * 0x01(1): flags/count? (when set has extra block data before start_offset)
     * 0x02(1): always 0?
     * 0x03(1): channels? (usually matches but rarely may be 0)
     * 0x04(4): some size, maybe >>2 ~= number of frames
     * 0x08(4): start offset
     * 0x0c(4): some sub-offset? (0x20, found when @0x01 is set) */

    /* use start offset as endianness flag */
    read_u32 = guess_read_u32(0x08,sf);

    uint32_t body_offset = read_u32(0x08,sf);
    uint8_t block_id = read_u8(body_offset, sf);


    if (block_id == EAAC_BLOCKID1_HEADER) {
        /* Dead Space 3 (PC) */
        info.sf_head = sf;
        info.head_offset = body_offset; /* header also at 0x10, but useless in SPS */
        info.type = meta_EA_SNU;
        info.is_sps = true;
    }
    else {
        info.sf_head = sf;
        info.sf_body = sf;
        info.head_offset = 0x10; /* SNR header */
        info.body_offset = body_offset; /* SNR body */
        info.type = meta_EA_SNU;
    }

    return load_vgmstream_ea_eaac(&info);
}
