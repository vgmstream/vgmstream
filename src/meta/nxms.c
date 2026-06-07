#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/endianness.h"
#include "../util/layout_utils.h"

#define SEGMENT_MAX 2

/* Arika wrapper [Tetris the Grand Master 4: Absolute Eye (Switch)] */
VGMSTREAM* init_vgmstream_nxms(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    segmented_layout_data* data = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "NXMS"))
        return NULL;
    if (!check_extensions(sf, "nxms"))
        return NULL;

    //04: type? (0x64=NXMS, 0x65=NXSE)
    int segment_count = read_u16le(0x08,sf);
    //0a: channels for all segments
    //0c: sample rate for all segments
    uint32_t info_start = read_u32le(0x10, sf);
    //14: data_start (unneeded since offsets are absolute)

    // segmented using 2 files (intro+loop, or just single file)
    if (segment_count < 1 || segment_count > SEGMENT_MAX)
         return NULL;
    int loop_start_segment = 1;
    int loop_end_segment = 1;
    int loop_flag = segment_count > 1;

    uint32_t offset = info_start;

    /* init layout */
    data = init_layout_segmented(segment_count);
    if (!data) goto fail;

    /* open each segment subfile */
    for (int i = 0; i < segment_count; i++) {
        uint32_t segment_offset = read_u32le(offset + 0x00, sf);
        uint32_t segment_size   = read_u32le(offset + 0x04, sf);
        int32_t segment_samples = read_s32le(offset + 0x08, sf);
        //0c: null
        //10: segment name (size 0x20), usually (filename)_head.nxms / (filename)_body.nxms

        STREAMFILE* temp_sf = setup_subfile_streamfile(sf, segment_offset, segment_size, "opus");
        if (!temp_sf) goto fail;

        data->segments[i] = init_vgmstream_opus_std(temp_sf);
        close_streamfile(temp_sf);
        if (!data->segments[i]) goto fail;

        data->segments[i]->num_samples = segment_samples;

        offset += 0x30;
    }

    /* setup segmented VGMSTREAMs */
    if (!setup_layout_segmented(data))
        goto fail;

    vgmstream = allocate_segmented_vgmstream(data, loop_flag, loop_start_segment, loop_end_segment);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_NXMS;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    free_layout_segmented(data);
    return NULL;
}
