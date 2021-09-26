#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* also see init_vgmstream_dsp_sps_n1 and init_vgmstream_opus_sps_n1 */

/* Nippon Ichi SPS wrapper [ClaDun (PSP), Legasista (PS3)] */
VGMSTREAM* init_vgmstream_sps_n1(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    int type, sample_rate;
    off_t subfile_offset;
    size_t subfile_size;

    init_vgmstream_t init_vgmstream = NULL;
    const char* extension = NULL;
    uint32_t (*read_u32)(off_t,STREAMFILE*);
    uint16_t (*read_u16)(off_t,STREAMFILE*);

    /* checks */
    if (!check_extensions(sf,"sps"))
        goto fail;

    if (guess_endianness32bit(0x00, sf)) { /* PS3 */
        read_u32 = read_u32be;
        read_u16 = read_u16be;
    }
    else {
        read_u32 = read_u32le;
        read_u16 = read_u16le;
    }
    type = read_u32(0x00,sf);
    subfile_size = read_u32(0x04,sf);
    sample_rate = read_u16(0x08,sf);
    /* 0x0a: flag? (stereo?) */
    /* 0x0b: flag? */
    /* 0x0c: num_samples */

    switch(type) {
        case 1:
            init_vgmstream = init_vgmstream_vag;
            extension = "vag";
            break;

        case 2:
            init_vgmstream = init_vgmstream_riff;
            extension = "at3";
            break;

        default:
            goto fail;
    }

    subfile_offset = 0x10;
    if (subfile_size + subfile_offset != get_streamfile_size(sf))
        goto fail;

    /* init the VGMSTREAM */
    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, extension);
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate; /* .vag header doesn't match */

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

/* Nippon Ichi SPS wrapper (segmented) [Penny-Punching Princess (Switch), Disgaea 4 Complete (PC)] */
VGMSTREAM* init_vgmstream_sps_n1_segmented(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t segment_offset;
    size_t data_size, max_size;
    int loop_flag, type, sample_rate;
    int i, segment;

    init_vgmstream_t init_vgmstream = NULL;
    const char* extension;
    segmented_layout_data* data = NULL;
    int segment_count, loop_start_segment, loop_end_segment;


    /* checks */
    /* .at9: Penny-Punching Princess (Switch)
     * .nlsd: Disgaea 4 Complete (PC) */
    if (!check_extensions(sf, "at9,nlsd"))
        goto fail;

    type = read_u32le(0x00,sf);
    data_size = read_u32le(0x04,sf);
    sample_rate = read_u16le(0x08,sf);
    /* 0x0a: flag? (stereo?) */
    /* 0x0b: flag? */
    /* 0x0c: num_samples (slightly smaller than added samples?) */

    switch(type) {
        case 7:
            init_vgmstream = init_vgmstream_ogg_vorbis;
            extension = "ogg";
            break;

        case 9:
            init_vgmstream = init_vgmstream_opus_std;
            extension = "opus";
            break;

        default:
            goto fail;
    }

    segment_offset = 0x1c;
    if (data_size + segment_offset != get_streamfile_size(sf))
        goto fail;

    /* segmented using 3 files (intro/loop/outro). non-segmented wrapper is the same
     * but with loop samples instead of sub-sizes */
    max_size = 0;
    segment_count = 0;
    for (i = 0; i < 3; i++) {
        size_t segment_size = read_u32le(0x10 + 0x04*i,sf);
        max_size += segment_size;
        /* may only set 1 segment (Disgaea4's bgm_185) */
        if (segment_size)
            segment_count++;
    }
    if (data_size != max_size)
        goto fail;

    loop_flag = segment_count > 1; /* intro+loop section must exit */
    loop_start_segment = 1;
    loop_end_segment = 1;

    /* init layout */
    data = init_layout_segmented(segment_count);
    if (!data) goto fail;

    /* open each segment subfile */
    segment = 0;
    for (i = 0; i < 3; i++) {
        STREAMFILE* temp_sf;
        size_t segment_size = read_u32le(0x10 + 0x04*i,sf);

        if (!segment_size)
            continue;

        temp_sf = setup_subfile_streamfile(sf, segment_offset,segment_size, extension);
        if (!temp_sf) goto fail;

        data->segments[segment] = init_vgmstream(temp_sf);
        close_streamfile(temp_sf);
        if (!data->segments[segment]) goto fail;

        segment_offset += segment_size;
        segment++;

        if (type == 9) {
            //todo there are some trailing samples that must be removed for smooth loops, start skip seems ok
            //not correct for all files, no idea how to calculate
            data->segments[segment]->num_samples -= 374;
        }
    }

    /* setup segmented VGMSTREAMs */
    if (!setup_layout_segmented(data))
        goto fail;

    vgmstream = allocate_segmented_vgmstream(data, loop_flag, loop_start_segment, loop_end_segment);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->meta_type = meta_SPS_N1;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    free_layout_segmented(data);
    return NULL;
}
