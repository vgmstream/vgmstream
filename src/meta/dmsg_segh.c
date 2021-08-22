#include "meta.h"
#include "../coding/coding.h"

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t offset;
    off_t current;
    off_t max;
    int le_type;
    int be_size;
    int full_size;
} chunk_t;

static int next_chunk(chunk_t* chunk, STREAMFILE* sf) {
    uint32_t (*read_u32type)(off_t,STREAMFILE*) = !chunk->le_type ? read_u32be : read_u32le;
    uint32_t (*read_u32size)(off_t,STREAMFILE*) = chunk->be_size ? read_u32be : read_u32le;

    if (chunk->max == 0)
        chunk->max = get_streamfile_size(sf);

    if (chunk->current >= chunk->max)
        return 0;
    /* can be used to signal "stop" */
    if (chunk->current < 0)
        return 0;

    chunk->type = read_u32type(chunk->current + 0x00,sf);
    chunk->size = read_u32size(chunk->current + 0x04,sf);

    chunk->offset = chunk->current + 0x04 + 0x04;
    chunk->current += chunk->full_size ? chunk->size : 0x08 + chunk->size;
    //;VGM_LOG("CHUNK: %x, %x, %x\n", dc.offset, chunk->type, chunk->size);

    /* read past data */
    if (chunk->type == 0xFFFFFFFF || chunk->size == 0xFFFFFFFF)
        return 0;

    /* empty chunk with 0 size, seen in some formats (XVAG uses it as end marker, Wwise doesn't) */
    if (chunk->type == 0 || chunk->size == 0)
        return 0;

    /* more chunks remain */
    return 1;
}

enum { 
    CHUNK_RIFF = 0x52494646, /* "RIFF" */
    CHUNK_LIST = 0x4C495354, /* "LIST" */
    CHUNK_segh = 0x73656768, /* "segh" */
};

/* DMSG - DirectMusic Segment with streams [Nightcaster II: Equinox (Xbox), Wildfire (PC)] */
VGMSTREAM* init_vgmstream_dmsg(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    //int loop_flag, channels, sample_rate;
    //int found_data = 0;
    //int32_t num_samples, loop_start, loop_end;
    //off_t start_offset;
    off_t offset = 0, name_offset = 0, name_size = 0;


    /* checks */
    /* .sgt: common
     * .dmsg: header id */
    if (!check_extensions(sf, "sgt,dmsg"))
        goto fail;

    if (!is_id32be(0x00,sf, "RIFF"))
        goto fail;
    if (!is_id32be(0x08,sf, "DMSG"))
        goto fail;

    /* A DirectMusic segment usually has lots of chunks then data pointing to .dls soundbank.
     * This accepts .sgt with a RIFF WAVE inside (less common). */
    {
        chunk_t rc = {0};
        chunk_t dc = {0};

        rc.current = 0x0c;
        while (next_chunk(&rc, sf)) {
            switch(rc.type) {
                /* "segh" has loopnum/samplesloop */
                case CHUNK_segh:
                    //todo: missing TMusicTime format
                    /* 0x00: dwRepeats (>0 or -1=inf)
                     * 0x04: mtLength
                     * 0x08: mtPlayStart
                     * 0x0c: mtLoopStart
                     * 0x10: mtLoopEnd
                     * 0x14: dwResolution
                     * 0x18: rtLength (optional)
                     * .. */
                    break;

                case CHUNK_LIST: 
                    if (is_id32be(rc.offset + 0x00, sf, "UNFO") && is_id32be(rc.offset + 0x04, sf, "UNAM")) {
                        name_offset = rc.offset + 0x0c;
                        name_size = read_u32le(rc.offset + 0x08, sf);
                    }
                    break;

                case CHUNK_RIFF:
                    if (!is_id32be(rc.offset, sf, "DMCN"))
                        goto fail;

                    dc.current = rc.offset + 0x04;
                    while (next_chunk(&dc, sf)) {
                        switch(dc.type) {
                            case CHUNK_LIST: 
                                /* abridged, there are some sublists */
                                if (is_id32be(dc.offset + 0x00, sf, "cosl") && is_id32be(dc.offset + 0x30, sf, "WAVE")) {
                                    offset = dc.offset + 0x34;
                                    dc.current = -1;
                                    rc.current = -1;
                                }
                                break;
                            default:
                                break;
                        }
                    }

                    break;
                default:
                    break;
            }
        }
    }

    if (!offset)
        goto fail;


    /* subfile has a few extra chunks (guid, wavh) but otherwise standard (seen PCM and MS-ADPCM, with fact chunks) */
    {
        STREAMFILE* temp_sf = NULL;
        off_t subfile_offset = offset;
        size_t subfile_size = read_u32le(offset + 0x04, sf) + 0x08;

        temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "wav");
        if (!temp_sf) goto fail;

        vgmstream = init_vgmstream_riff(temp_sf);
        close_streamfile(temp_sf);
        if (!vgmstream) goto fail;

        if (name_offset) {
            if (name_size >= STREAM_NAME_SIZE)
                name_size = STREAM_NAME_SIZE;
            read_string_utf16le(vgmstream->stream_name,name_size, name_offset, sf);
        }

        return vgmstream;
    }

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
