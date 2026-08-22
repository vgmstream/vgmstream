#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/endianness.h"
#include "../util/layout_utils.h"

#define MAX_SECTIONS 3

static int load_segment_config(int type, uint32_t data_offset, uint32_t data_size, uint32_t* section_sizes, uint32_t* segment_offsets, uint32_t* segment_sizes) {

    // older 9=opusnx games use full segments to handle looping while later they use samples,
    // autodetect (7=ogg always seems segmented)
    if (!(type == 7 || (type == 9 && data_offset == 0x1C)))
        return 0;

    // load segment offsets+sizes
    if (data_offset == 0x1c) {
        // section values are sizes [Penny-Punching Princess (Switch), Disgaea 4 Complete Plus (PC)]
        uint32_t offset = data_offset;
        for (int i = 0; i < MAX_SECTIONS; i++) {
            uint32_t segment_size = section_sizes[i];

            segment_sizes[i] = segment_size;
            segment_offsets[i] = offset;
            offset += segment_sizes[i];
        }
    }
    else if (data_offset == 0x18) {
        // section values are offsets [Labyrinth of Galleria (PC)]
        uint32_t offset = data_offset;
        for (int i = 0; i < MAX_SECTIONS; i++) {
            uint32_t next_offset;
            if (i >= 2) { // only 2 (not sure if it can be 0)
                next_offset = data_offset + data_size;
            }
            else {
                next_offset = data_offset + section_sizes[i];
            }

            segment_sizes[i] = (next_offset - offset);
            segment_offsets[i] = offset;
            offset += segment_sizes[i];
        }
    }
    else {
        return 0;
    }

    uint32_t max_size = 0;
    int segment_count = 0;
    for (int i = 0; i < MAX_SECTIONS; i++) {
        // may only set 1 segment, with empty intro/outro (Disgaea4's bgm_185)
        if (segment_sizes[i])
            segment_count++;
        max_size += segment_sizes[i];
    }

    if (max_size != data_size)
        return 0;

    return segment_count;
}


/* System Prisma (later Nippon Ichi) SPS wrapper [ClaDun (PSP), Legasista (PS3)] */
VGMSTREAM* init_vgmstream_sps_n1(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    segmented_layout_data* data = NULL;
    init_vgmstream_t init_vgmstream = NULL;
    const char* extension = NULL;

    /* checks */
    uint32_t type = read_u32le(0x00,sf);
    if (!(type == 0x01000000 || type == 0x02000000 || (type >= 1 && type <= 9)))
        return NULL;

    /* .sps: ClaDun (PSP), Legasista (PS3), ClaDun X3 (Switch), Labyrinth of Refrain: Coven of Dusk (Switch)
     * .nlsd: Ys VIII (Switch), Disgaea Refine (Switch), Disgaea 4 Complete (PC)-segmented
     * .vag: Penny-Punching Princess (Switch)
     * .at9: void tRrLM() (Swithc), Penny-Punching Princess (Switch)-segmented, Labyrinth of Galleria (PC)-segmented 
     * .opus: Asatsugutori (Switch) */
    if (!check_extensions(sf,"sps,nlsd,vag,at9,opus,lopus"))
        return NULL;


    bool is_be = guess_endian32(0x00, sf);
    read_u32_t read_u32 = get_read_u32(is_be);
    read_u16_t read_u16 = get_read_u16(is_be);
    uint32_t file_size = get_streamfile_size(sf);

    type                = read_u32(0x00,sf);
    uint32_t data_size  = read_u32(0x04,sf);
    int sample_rate     = read_u16(0x08,sf);
    int flag1           =  read_u8(0x0a,sf); // usually 0/1 (stereo flag?)
    int flag2           =  read_u8(0x0b,sf); // rarely 0/1, doesn't seem related to loops [Cladun X2 (PSP), Legasista (PS3)]
    uint32_t num_samples= read_u32(0x0c,sf); // slightly smaller than added samples in segmented cases?
    if (sample_rate < 8000 || sample_rate > 48000) //arbitrary max
        return NULL;
    if (flag1 > 0x01 || flag2 > 0x01)
        return NULL;
    if (data_size > file_size)
        return NULL;

    uint32_t data_offset = file_size - data_size;
    uint32_t section_sizes[MAX_SECTIONS] = {0};
    switch (data_offset) {
        case 0x10: // old games
            break;

        case 0x1c: // ~2018 games (values should add up to num_samples or data_size)
            section_sizes[0] = read_u32(0x10,sf); // intro samples / size
            section_sizes[1] = read_u32(0x14,sf); // body samples / size
            section_sizes[2] = read_u32(0x18,sf); // outro samples / size
            break;

        case 0x18: // ~2023 games
            section_sizes[0] = read_u32(0x10,sf); // loop start / body offset (within data)
            section_sizes[1] = read_u32(0x14,sf); // loop end / outro offset (within data)
            // section 3/outro exists but is implicit (num_samples / data end)
            break;

        default:
            return NULL;
    }   

    switch (type) {
        case 1:
            init_vgmstream = init_vgmstream_vag;
            extension = "vag";
            break;

        case 2:
            init_vgmstream = init_vgmstream_riff;
            extension = "at3";
            break;

        case 7:
            init_vgmstream = init_vgmstream_ogg_vorbis;
            extension = "ogg";
            break;

        case 8:
            init_vgmstream = init_vgmstream_ngc_dsp_std_le;
            extension = "adpcm";
            break;

        case 9:
            init_vgmstream = init_vgmstream_opus_std;
            extension = "opus";
            break;

        default:
            return NULL;
    }


    uint32_t segment_offsets[MAX_SECTIONS] = {0};
    uint32_t segment_sizes[MAX_SECTIONS] = {0};
    int segment_count = load_segment_config(type, data_offset, data_size, section_sizes, segment_offsets, segment_sizes);

    /* init the VGMSTREAM */
    if (segment_count) {
        int segment;

        bool loop_flag = true; /* intro+loop section must exist */
        int loop_start_segment = 1;
        int loop_end_segment = 1;

        /* init layout */
        data = init_layout_segmented(segment_count);
        if (!data) goto fail;

        /* open each segment subfile */
        segment = 0;
        for (int i = 0; i < MAX_SECTIONS; i++) {
            VGMSTREAM* vs = NULL;
            STREAMFILE* temp_sf = NULL;

            if (!segment_sizes[i])
                continue;

            temp_sf = setup_subfile_streamfile(sf, segment_offsets[i], segment_sizes[i], extension);
            if (!temp_sf) goto fail;

            vs = init_vgmstream(temp_sf);
            close_streamfile(temp_sf);
            temp_sf = NULL;
            if (!vs) goto fail;

            if (type == 9) {
                //TODO there are some trailing samples that must be removed for smooth loops, start skip seems ok
                //not correct for all files, no idea how to calculate
                vs->num_samples -= 374;
            }

            data->segments[segment] = vs;
            segment++;
        }

        if (!setup_layout_segmented(data))
            goto fail;

        vgmstream = allocate_segmented_vgmstream(data, loop_flag, loop_start_segment, loop_end_segment);
        if (!vgmstream) goto fail;
    }
    else {
        STREAMFILE* temp_sf = NULL;
        bool loop_flag = false;
        int32_t loop_start = 0;
        int32_t loop_end = 0;

        if (data_offset == 0x1c) {
            loop_start = section_sizes[0];
            loop_end = section_sizes[1] + loop_start;
            loop_flag = loop_start > 0 || section_sizes[2] > 0;
        }
        else if (data_offset == 0x18) {
            loop_start = section_sizes[0];
            loop_end = section_sizes[1];
            loop_flag = loop_start != loop_end; // with loop disabled start and end are the same as num samples
            
            // most files set loops in DSP header but don't loop, though some do full loops
            if (type == 8 && loop_start == 0) {
                loop_flag = false;
            }
        }

        temp_sf = setup_subfile_streamfile(sf, data_offset, data_size, extension);
        if (!temp_sf) goto fail;

        vgmstream = init_vgmstream(temp_sf);
        close_streamfile(temp_sf);
        temp_sf = NULL;
        if (!vgmstream) goto fail;

        if (data_offset != 0x10) {
            vgmstream_force_loop(vgmstream, loop_flag, loop_start, loop_end);
        }

        // TODO: test if header num_samples is actually used.
        // Some 1=vag and 9=opus set slightly less samples (100-600), but may also be lower
        // than cald'c loop samples in PS-ADPCM. Other types seem to match always.
        if (vgmstream->num_samples > num_samples && !vgmstream->loop_flag) {
            vgmstream->num_samples = num_samples;
        }
    }

    // some 1=vag internal header is slighty different vs .sps info
    vgmstream->sample_rate = sample_rate; 

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
