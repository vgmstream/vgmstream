#include "meta.h"
#include "../layout/layout.h"
#include "aix_streamfile.h"


/* usually segment0=intro, segment1=loop/main, sometimes ~5, rarely ~40~115
 * as pseudo dynamic/multi-song container [Sega Ages 2500 Vol 28 Tetris Collection (PS2)] */
#define MAX_SEGMENTS 120

static VGMSTREAM* build_segmented_vgmstream(STREAMFILE* sf, off_t* segment_offsets, size_t* segment_sizes, int32_t* segment_samples, int segment_count, int layer_count);

/* AIX - N segments with M layers (2ch ADX) inside [SoulCalibur IV (PS3), Dragon Ball Z: Burst Limit (PS3)] */
VGMSTREAM* init_vgmstream_aix(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;

    off_t segment_offsets[MAX_SEGMENTS] = {0};
    size_t segment_sizes[MAX_SEGMENTS] = {0};
    int32_t segment_samples[MAX_SEGMENTS] = {0};
    int segment_rates[MAX_SEGMENTS] = {0};

    off_t data_offset, subtable_offset;
    int segment_count, layer_count;
    int i;


    /* checks */
    if (!check_extensions(sf, "aix"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x41495846 ||  /* "AIXF" */
        read_u32be(0x08,sf) != 0x01000014 ||  /* version? */
        read_u32be(0x0c,sf) != 0x00000800)
        goto fail;

    /* AIX combine layers for multichannel and segments for looping, all very hacky.
     * For some reason AIX with 1 layer and 1 segment exist (equivalent to a single ADX). */

    /* base segment header */
    data_offset = read_s32be(0x04,sf) + 0x08;

    /* parse segments table */
    {
        const off_t segment_list_offset = 0x20;
        const size_t segment_list_entry_size = 0x10;

        segment_count = read_u16be(0x18,sf);
        if (segment_count < 1 || segment_count > MAX_SEGMENTS) goto fail;

        subtable_offset = segment_list_offset + segment_count*segment_list_entry_size;
        if (subtable_offset >= data_offset) goto fail;

        for (i = 0; i < segment_count; i++) {
            segment_offsets[i] = read_s32be(segment_list_offset + segment_list_entry_size*i + 0x00,sf);
            segment_sizes[i]   = read_u32be(segment_list_offset + segment_list_entry_size*i + 0x04,sf);
            segment_samples[i] = read_s32be(segment_list_offset + segment_list_entry_size*i + 0x08,sf);
            segment_rates[i]   = read_s32be(segment_list_offset + segment_list_entry_size*i + 0x0c,sf);

            /* segments > 0 can have 0 sample rate, seems to indicate same as first
             * [Ryu ga Gotoku: Kenzan! (PS3) tenkei_sng1.aix] */
            if (i > 0 && segment_rates[i] == 0)
                segment_rates[i] = segment_rates[0];

            /* all segments must have equal sample rate */
            if (segment_rates[i] != segment_rates[0])
                goto fail;
        }

        if (segment_offsets[0] != data_offset)
            goto fail;
    }

    /* between the segment and layer table some kind of 0x10 subtable? */
    if (read_u8(subtable_offset,sf) != 0x01)
        goto fail;

    /* parse layers table */
    {
        const size_t layer_list_entry_size = 0x08;
        off_t layer_list_offset, layer_list_end;

        layer_list_offset = subtable_offset + 0x10;
        if (layer_list_offset >= data_offset) goto fail;

        layer_count = read_u8(layer_list_offset,sf);
        if (layer_count < 1) goto fail;

        layer_list_end = layer_list_offset + 0x08 + layer_count*layer_list_entry_size;
        if (layer_list_end >= data_offset) goto fail;

        for (i = 0; i < layer_count; i++) {
            /* all layers must have same sample rate as segments */
            if (read_s32be(layer_list_offset + 0x08 + i*layer_list_entry_size + 0x00,sf) != segment_rates[0])
                goto fail;
            /* 0x04: layer channels */
        }
    }


    /* build combo layers + segments VGMSTREAM */
    vgmstream = build_segmented_vgmstream(sf, segment_offsets, segment_sizes, segment_samples, segment_count, layer_count);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_AIX;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

static VGMSTREAM *build_layered_vgmstream(STREAMFILE* sf, off_t segment_offset, size_t segment_size, int layer_count) {
    VGMSTREAM* vgmstream = NULL;
    layered_layout_data* data = NULL;
    int i;
    STREAMFILE* temp_sf = NULL;


    /* build layers */
    data = init_layout_layered(layer_count);
    if (!data) goto fail;

    for (i = 0; i < layer_count; i++) {
        /* build the layer STREAMFILE */
        temp_sf = setup_aix_streamfile(sf, segment_offset, segment_size, i, "adx");
        if (!temp_sf) goto fail;

        /* build the sub-VGMSTREAM */
        data->layers[i] = init_vgmstream_adx(temp_sf);
        if (!data->layers[i]) goto fail;

        data->layers[i]->stream_size = get_streamfile_size(temp_sf);

        close_streamfile(temp_sf);
        temp_sf = NULL;
    }

    if (!setup_layout_layered(data))
        goto fail;


    /* build the layered VGMSTREAM */
    vgmstream = allocate_layered_vgmstream(data);
    if (!vgmstream) goto fail;

    return vgmstream;

fail:
    if (!vgmstream) free_layout_layered(data);
    close_vgmstream(vgmstream);
    close_streamfile(temp_sf);
    return NULL;
}

static VGMSTREAM *build_segmented_vgmstream(STREAMFILE* sf, off_t* segment_offsets, size_t* segment_sizes, int32_t* segment_samples, int segment_count, int layer_count) {
    VGMSTREAM* vgmstream = NULL;
    segmented_layout_data* data = NULL;
    int i, loop_flag, loop_start_segment, loop_end_segment;


    /* build segments */
    data = init_layout_segmented(segment_count);
    if (!data) goto fail;

    for (i = 0; i < segment_count; i++) {
        /* build the layered sub-VGMSTREAM */
        data->segments[i] = build_layered_vgmstream(sf, segment_offsets[i], segment_sizes[i], layer_count);
        if (!data->segments[i]) goto fail;

        data->segments[i]->num_samples = segment_samples[i]; /* just in case */

        data->segments[i]->stream_size = segment_sizes[i];
    }

    if (!setup_layout_segmented(data))
        goto fail;

    /* known loop cases:
     * - 1 segment: main/no loop [Hatsune Miku: Project Diva (PSP)]
     * - 2 segments: intro + loop [SoulCalibur IV (PS3)]
     * - 3 segments: intro + loop + end [Dragon Ball Z: Burst Limit (PS3), Metroid: Other M (Wii)]
     * - 4/5 segments: intros + loop + ends [Danball Senki (PSP)]
     * - +39 segments: no loops but multiple segments for dynamic parts? [Tetris Collection (PS2)] */
    loop_flag = (segment_count > 0 && segment_count <= 5);
    loop_start_segment = (segment_count > 3) ? 2 : 1;
    loop_end_segment = (segment_count > 3) ? (segment_count - 2) : 1;

    /* build the segmented VGMSTREAM */
    vgmstream = allocate_segmented_vgmstream(data, loop_flag, loop_start_segment, loop_end_segment);
    if (!vgmstream) goto fail;

    return vgmstream;

fail:
    if (!vgmstream) free_layout_segmented(data);
    close_vgmstream(vgmstream);
    return NULL;
}
