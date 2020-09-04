#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "sab_streamfile.h"

typedef struct {
    int total_subsongs;
    int target_subsong;
    int is_stream;
    int is_extra;

    uint32_t flags;
    uint32_t sound_count;
    uint32_t block_size;

    uint32_t codec;
    int loop_flag;
    int channel_count;
    int sample_rate;
    uint32_t stream_size;
    int32_t loop_start;
    int32_t loop_end;
    off_t stream_offset;
} sab_header;


static int parse_sab(STREAMFILE* sf, sab_header* sab);
static VGMSTREAM* build_layered_vgmstream(STREAMFILE* sf, sab_header* sab);
static void get_stream_name(char* stream_name, STREAMFILE* sf, int target_stream);

/* SAB - from Sensaura GameCODA middleware games [Men of Valor (multi), Worms 4: Mayhem (multi), Just Cause (multi)] */
VGMSTREAM* init_vgmstream_sab(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    sab_header sab = {0};


    /* .sab: main, .sob: cue (has config/names) */
    if (!check_extensions(sf,"sab"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x43535732 &&  /* "CSW2" (Windows) */
        read_u32be(0x00,sf) != 0x43535032 &&  /* "CSP2" (PS2) */
        read_u32be(0x00,sf) != 0x43535832)    /* "CSX2" (Xbox) */
        goto fail;

    if (!parse_sab(sf, &sab))
        goto fail;

    /* sab can be (both cases handled here):
     * - bank: multiple subsongs (each a different header)
     * - stream: layers used as different stereo tracks (Men of Valor) or mono tracks to create stereo (Worms 4)
     */
    vgmstream = build_layered_vgmstream(sf, &sab);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = sab.total_subsongs;

    get_stream_name(vgmstream->stream_name, sf, sab.target_subsong);

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

//todo doesn't seem correct always (also multiple .sob may share .sab)
/* multiple streams may share a name */
static void get_stream_name(char* stream_name, STREAMFILE* sf, int target_stream) {
    STREAMFILE* sf_info = NULL;
    int i, j, total_cues, num_cue = -1;
    size_t name_size = 0;
    off_t name_offset = 0x10;

    sf_info = open_streamfile_by_ext(sf, "sob");
    if (!sf_info) goto end;
    if (read_u32be(0x00,sf_info) != 0x43544632) /* "CTF2" */
        goto end;

    total_cues = read_u32le(0x08,sf_info);
    if (total_cues > 0x1000)
        goto end;

    for (i = 0; i < total_cues; i++) {
        uint32_t flags, num_subsections, subsection_1_size, subsection_2_size;

        flags             = read_u32le(name_offset + 0x00,sf_info);
        num_subsections   = read_u32le(name_offset + 0x20,sf_info);
        subsection_1_size = (flags & 0x00000001) ? 0x40 : 0x00;
        subsection_1_size +=(flags & 0x00000040) ? 0x20 : 0x00;
        subsection_2_size = (flags & 0x00000100) ? 0x1c : 0x10; //todo not always correct

        if (num_subsections > 0x1000)
            goto end; /* bad read */

        for (j = 0; j < num_subsections; j++) {
            int num_stream = read_u32le(name_offset + 0x2c + subsection_1_size + j*subsection_2_size + 0x08,sf_info);
            if (target_stream - 1 == num_stream)
                num_cue = i;
        }

        name_offset += 0x2c + subsection_1_size + subsection_2_size * num_subsections;
    }
    if (num_cue < 0)
        goto end;

    for (i = 0; i < total_cues; i++) {
        /* 0x00: id */
        name_size = read_u32le(name_offset + 0x04,sf_info); /* non null-terminated */
        if (i == num_cue) {
            name_offset += 0x08;
            break;
        }
        name_offset += 0x08 + name_size;
    }

    if (name_size > STREAM_NAME_SIZE - 1)
        name_size = STREAM_NAME_SIZE - 1;

    read_string(stream_name,name_size+1, name_offset,sf_info);

end:
    close_streamfile(sf_info);
}

static int parse_sab(STREAMFILE* sf, sab_header* sab) {
    off_t entry_offset;

    sab->flags       = read_u32le(0x04,sf); /* upper byte is always 0x02 (version?) */
    sab->sound_count = read_u32le(0x08,sf);
    sab->block_size  = read_u32le(0x0c,sf);
    /* 0x10: number of blocks */
    /* 0x14: file id? */

    sab->is_stream = sab->flags & 0x04; /* "not bank" */
    sab->is_extra  = sab->flags & 0x10; /* not used in Worms 4 */
    /* flags 1/2 are also common, no flags in banks */

    sab->total_subsongs = sab->is_stream ? 1 : sab->sound_count;
    sab->target_subsong = sf->stream_index;
    if (sab->target_subsong == 0) sab->target_subsong = 1;
    if (sab->target_subsong < 0 || sab->target_subsong > sab->total_subsongs || sab->total_subsongs < 1) goto fail;

    /* stream config */
    entry_offset = 0x18 + 0x1c * (sab->target_subsong - 1);
    sab->codec         = read_u32le(entry_offset + 0x00,sf);
    sab->channel_count = read_u32le(entry_offset + 0x04,sf);
    sab->sample_rate   = read_u32le(entry_offset + 0x08,sf);
    sab->stream_size   = read_u32le(entry_offset + 0x0c,sf);
    sab->loop_start    = read_u32le(entry_offset + 0x10,sf);
    sab->loop_end      = read_u32le(entry_offset + 0x14,sf);
    sab->loop_flag     = (sab->loop_end > 0);

    if (sab->is_stream) {
        sab->stream_offset = sab->block_size;
    }
    else {
        sab->stream_offset  = 0x18 + 0x1c * sab->total_subsongs;
        if (sab->stream_offset % sab->block_size)
            sab->stream_offset += sab->block_size - (sab->stream_offset % sab->block_size);
        sab->stream_offset += read_u32le(0x18 + 0x1c * (sab->target_subsong - 1) + 0x18,sf);
    }

    /* some extra values (counts?) and name at start */
    if (sab->is_extra)
        sab->stream_offset += sab->block_size;

    return 1;
fail:
    return 0;
}

static VGMSTREAM* build_vgmstream(STREAMFILE* sf, sab_header* sab) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;

    /* in streams streamfile is de-chunked so start becomes 0 */
    start_offset = sab->is_stream ?
            0 :
            sab->stream_offset;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(sab->channel_count, sab->loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sab->sample_rate;
    vgmstream->stream_size = sab->stream_size;
    vgmstream->meta_type = meta_SAB;

    switch(sab->codec) {
        case 0x01: /* PC */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            vgmstream->num_samples = pcm_bytes_to_samples(sab->stream_size, vgmstream->channels, 16);
            vgmstream->loop_start_sample = pcm_bytes_to_samples(sab->loop_start, vgmstream->channels, 16);
            vgmstream->loop_end_sample = pcm_bytes_to_samples(sab->loop_end, vgmstream->channels, 16);
            break;

        case 0x04: /* PS2 */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            vgmstream->num_samples = ps_bytes_to_samples(sab->stream_size, vgmstream->channels);
            vgmstream->loop_start_sample = ps_bytes_to_samples(sab->loop_start, vgmstream->channels);
            vgmstream->loop_end_sample = ps_bytes_to_samples(sab->loop_end, vgmstream->channels);
            break;

        case 0x08: /* Xbox */
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = xbox_ima_bytes_to_samples(sab->stream_size, vgmstream->channels);
            vgmstream->loop_start_sample = xbox_ima_bytes_to_samples(sab->loop_start, vgmstream->channels);
            vgmstream->loop_end_sample = xbox_ima_bytes_to_samples(sab->loop_end, vgmstream->channels);
            break;

        default:
            VGM_LOG("SAB: unknown codec\n");
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

static VGMSTREAM* build_layered_vgmstream(STREAMFILE* sf, sab_header* sab) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    layered_layout_data* data = NULL;
    int i;

    if (sab->sound_count == 1 || !sab->is_stream) {
        return build_vgmstream(sf, sab);
    }

    /* init layout */
    data = init_layout_layered(sab->sound_count);
    if (!data) goto fail;

    /* de-chunk audio layers */
    for (i = 0; i < sab->sound_count; i++) {
        temp_sf = setup_sab_streamfile(sf, sab->stream_offset, sab->sound_count, i, sab->block_size);
        if (!temp_sf) goto fail;

        data->layers[i] = build_vgmstream(temp_sf, sab);
        if (!data->layers[i]) goto fail;

        close_streamfile(temp_sf);
        temp_sf = NULL;
    }

    /* setup VGMSTREAMs */
    if (!setup_layout_layered(data))
        goto fail;

    /* build the layout VGMSTREAM */
    vgmstream = allocate_layered_vgmstream(data);
    if (!vgmstream) goto fail;

    //sab->stream_size = sab->stream_size * sab->sound_count; /* ? */

    return vgmstream;
fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    if (!vgmstream)
        free_layout_layered(data);
    return NULL;
}
