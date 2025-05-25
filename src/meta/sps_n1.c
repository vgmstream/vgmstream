#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/endianness.h"
#include "../util/layout_utils.h"

/* Nippon Ichi SPS wrapper [ClaDun (PSP), Legasista (PS3)] */
VGMSTREAM* init_vgmstream_sps_n1(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    init_vgmstream_t init_vgmstream = NULL;
    const char* extension = NULL;
    read_u32_t read_u32;
    read_u16_t read_u16;

    /* checks */
    int type = read_u32le(0x00,sf);
    if (!((type > 0 && type < 10) || type == 0x01000000 || type == 0x02000000))
        return NULL;

    /* .sps: common, ClaDun X3 (Switch)
     * .vag: Penny-Punching Princess (Switch)
     * .nlsd: Ys VIII (Switch) */
    if (!check_extensions(sf,"sps,vag,nlsd"))
        return NULL;

    if (guess_endian32(0x00, sf)) { // PS3
        read_u32 = read_u32be;
        read_u16 = read_u16be;
    }
    else {
        read_u32 = read_u32le;
        read_u16 = read_u16le;
    }

    type = read_u32(0x00,sf);
    uint32_t subfile_size = read_u32(0x04,sf);
    int sample_rate = read_u16(0x08,sf);
    // 0x0a: flag? (stereo?)
    // 0x0b: flag?
    // 0x0c: num_samples
    // newer games with loop section (>7?):
    // 0x10: loop start
    // 0x14: loop end
    // some games extra value (>8?):
    // 0x18: null?

    if (sample_rate < 8000 || sample_rate > 48000) //arbitrary max
        return NULL;

    uint32_t subfile_offset = get_streamfile_size(sf) - subfile_size;
    if (subfile_offset <= 0 || subfile_offset > 0x1c)
        return NULL;

    switch(type) {
        case 1:
            init_vgmstream = init_vgmstream_vag;
            extension = "vag";
            break;

        case 2:
            init_vgmstream = init_vgmstream_riff;
            extension = "at3";
            break;

        case 8:
            init_vgmstream = init_vgmstream_ngc_dsp_std_le;
            extension = "adpcm";
            break;

        case 7:
            // init_vgmstream_ogg_vorbis (init_vgmstream_sps_n1_segmented)
        case 9:
            // init_vgmstream_opus_sps_n1
            // init_vgmstream_opus_std (init_vgmstream_sps_n1_segmented)

        default:
            return NULL;
    }


    /* init the VGMSTREAM */
    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, extension);
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate; // internal header isn't always correct vs .sps info

    if (type == 8) {
        // most files set loops in DSP header but don't loop, though some do full loops
        uint32_t loop_start = read_u32(0x10, sf);
        if (loop_start == 0)
            vgmstream->loop_flag = false;
    }

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

#define SEGMENT_MAX 3

/* Nippon Ichi SPS wrapper (segmented) [Penny-Punching Princess (Switch), Disgaea 4 Complete (PC)] */
VGMSTREAM* init_vgmstream_sps_n1_segmented(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    size_t data_size, max_size;
    int loop_flag, type, sample_rate;

    init_vgmstream_t init_vgmstream = NULL;
    const char* extension;
    segmented_layout_data* data = NULL;
    int loop_start_segment, loop_end_segment;


    /* checks */
    type = read_u32le(0x00,sf);
    if (type < 7 || type > 10)
        return NULL;

    /* .at9: Penny-Punching Princess (Switch), Labyrinth of Galleria (PC)
     * .nlsd: Disgaea 4 Complete (PC) */
    if (!check_extensions(sf, "at9,nlsd"))
        return NULL;

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
            return NULL;
    }

    /* segmented using 3 files (intro/loop/outro). non-segmented wrapper is the same
     * but with loop samples instead of sub-sizes */

    uint32_t segment_offsets[SEGMENT_MAX];
    uint32_t segment_sizes[SEGMENT_MAX];
    uint32_t segment_start, offset;
    int segment_count, segment;

    if (data_size + 0x1c == get_streamfile_size(sf)) {
        /* common */
        segment_start = 0x1c;
        offset = segment_start;
        for (int i = 0; i < SEGMENT_MAX; i++) {
            uint32_t segment_size = read_u32le(0x10 + 0x04*i,sf);

            segment_sizes[i] = segment_size;
            segment_offsets[i] = offset;
            offset += segment_sizes[i];
        }
    }
    else if (data_size + 0x18 == get_streamfile_size(sf)) {
        /* Labyrinth of Galleria (PC) */
        segment_start = 0x18;
        offset = segment_start;
        for (int i = 0; i < SEGMENT_MAX; i++) {
            uint32_t next_offset;
            if (i >= 2) {
                next_offset = get_streamfile_size(sf) - segment_start;
            }
            else {
                next_offset = read_u32le(0x10 + 0x04*i,sf); /* only 2 (not sure if it can be 0) */
            }

            segment_sizes[i] = next_offset - offset + segment_start;
            segment_offsets[i] = offset;
            offset += segment_sizes[i];
        }
    }
    else {
        goto fail;
    }

    max_size = 0;
    segment_count = 0;
    for (int i = 0; i < SEGMENT_MAX; i++) {
        /* may only set 1 segment, with empty intro/outro (Disgaea4's bgm_185) */
        if (segment_sizes[i])
            segment_count++;
        max_size += segment_sizes[i];
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
    for (int i = 0; i < SEGMENT_MAX; i++) {
        if (!segment_sizes[i])
            continue;

        STREAMFILE* temp_sf = setup_subfile_streamfile(sf, segment_offsets[i],segment_sizes[i], extension);
        if (!temp_sf) goto fail;

        data->segments[segment] = init_vgmstream(temp_sf);
        close_streamfile(temp_sf);
        if (!data->segments[segment]) goto fail;

        segment++;

        if (type == 9) {
            //TODO there are some trailing samples that must be removed for smooth loops, start skip seems ok
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
