#include "meta.h"
#include "../util/endianness.h"

#define EAAC_BLOCKID1_HEADER            0x48 /* 'H' */


/* EA SBR/SBS - used in older 7th gen games for storing SFX */
VGMSTREAM* init_vgmstream_ea_sbr(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE *sf_sbs = NULL;
    uint32_t num_sounds, sound_id, type_desc, num_items, item_type,
        table_offset, types_offset, entry_offset, items_offset, data_offset, snr_offset, sns_offset;
    int target_stream = sf->stream_index;
    eaac_meta_t info = {0};


    /* checks */
    if (!is_id32be(0x00, sf, "SBKR"))
        return NULL;
    if (!check_extensions(sf, "sbr"))
        return NULL;

    /* SBR files are always big endian */
    num_sounds = read_u32be(0x1c, sf);
    table_offset = read_u32be(0x24, sf);
    types_offset = read_u32be(0x28, sf);

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || num_sounds == 0 || target_stream > num_sounds)
        goto fail;

    entry_offset = table_offset + 0x0a * (target_stream - 1);
    sound_id = read_u32be(entry_offset + 0x00, sf);
    num_items = read_u16be(entry_offset + 0x04, sf);
    items_offset = read_u32be(entry_offset + 0x06, sf);

    snr_offset = 0;
    sns_offset = 0;

    for (uint32_t i = 0; i < num_items; i++) {
        entry_offset = items_offset + 0x06 * i;
        item_type = read_u16be(entry_offset + 0x00, sf);
        data_offset = read_u32be(entry_offset + 0x02, sf);

        type_desc = read_u32be(types_offset + 0x06 * item_type, sf);

        switch (type_desc) {
            case 0x534E5231: /* "SNR1" */
                snr_offset = data_offset;
                break;
            case 0x534E5331: /* "SNS1" */
                sns_offset = read_u32be(data_offset, sf);
                break;
            default:
                break;
        }
    }

    if (snr_offset == 0 && sns_offset == 0)
        goto fail;

    if (sns_offset == 0) {
        /* RAM asset */
        
        info.sf_head = sf;
        info.head_offset = snr_offset;
        info.body_offset = 0x00;
        info.type = (read_u8(snr_offset, sf) == EAAC_BLOCKID1_HEADER) ? meta_EA_SPS : meta_EA_SNR_SNS;;

        vgmstream = load_vgmstream_ea_eaac(&info);
        if (!vgmstream) goto fail;
    }
    else {
        /* streamed asset */
        sf_sbs = open_streamfile_by_ext(sf, "sbs");
        if (!sf_sbs) goto fail;

        if (!is_id32be(0x00, sf_sbs, "SBKS"))
            goto fail;

        if (read_u8(sns_offset, sf_sbs) == EAAC_BLOCKID1_HEADER) {
            /* SPS */
            info.sf_head = sf_sbs;
            info.head_offset = sns_offset;
            info.body_offset = 0x00;
            info.type = meta_EA_SPS;

            vgmstream = load_vgmstream_ea_eaac(&info);
            if (!vgmstream) goto fail;
        }
        else {
            /* SNR/SNS */
            if (snr_offset == 0)
                goto fail;

            info.sf_head = sf;
            info.sf_body = sf_sbs;
            info.head_offset = snr_offset;
            info.body_offset = sns_offset;
            info.type = meta_EA_SNR_SNS;

            vgmstream = load_vgmstream_ea_eaac(&info);
            if (!vgmstream) goto fail;
        }
    }

    snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%08x", sound_id);
    vgmstream->num_streams = num_sounds;

    close_streamfile(sf_sbs);
    return vgmstream;
fail:
    close_streamfile(sf_sbs);
    return NULL;
}
