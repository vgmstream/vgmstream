#include "meta.h"
#include "../coding/coding.h"
#include "../util/chunks.h"

typedef struct {
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t dpds_offset;
    uint32_t dpds_size;

    int loop_flag;

    int format;
    int channels;
    int sample_rate;
    int bytes;
    int avg_bitrate;
    int block_size;
} xwma_header_t;


/* XWMA - Microsoft WMA container [The Elder Scrolls: Skyrim (PC/X360), Hydrophobia (PC)]  */
VGMSTREAM* init_vgmstream_xwma(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    xwma_header_t xwma = {0};


    /* checks */
    if (!is_id32be(0x00,sf, "RIFF"))
        goto fail;
    if (!is_id32be(0x08,sf, "XWMA"))
        goto fail;
    /* .xwma: standard
     * .xwm: The Elder Scrolls: Skyrim (PC), Blade Arcus from Shining (PC) */
    if (!check_extensions(sf, "xwma,xwm"))
        goto fail;

    {
        enum { 
            CHUNK_fmt  = 0x666d7420, /* "fmt " */
            CHUNK_data = 0x64617461, /* "data" */
            CHUNK_dpds = 0x64706473, /* "dpds" */
        };
        chunk_t rc = {0};

        rc.current = 0x0c;
        while (next_chunk(&rc, sf)) {
            switch(rc.type) {
                case CHUNK_fmt:
                    xwma.format      = read_u16le(rc.offset+0x00, sf);
                    xwma.channels    = read_u16le(rc.offset+0x02, sf);
                    xwma.sample_rate = read_u32le(rc.offset+0x04, sf);
                    xwma.avg_bitrate = read_u32le(rc.offset+0x08, sf);
                    xwma.block_size  = read_u16le(rc.offset+0x0c, sf);
                    break;

                case CHUNK_data:
                    xwma.data_offset = rc.offset;
                    xwma.data_size = rc.size;
                    break;

                case CHUNK_dpds:
                    xwma.dpds_offset = rc.offset;
                    xwma.dpds_size = rc.size;
                    break;

                default:
                    break;
            }
        }
        if (!xwma.format || !xwma.data_offset)
            goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(xwma.channels, xwma.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XWMA;
    vgmstream->sample_rate = xwma.sample_rate;

    /* the main purpose of this meta is redoing the XWMA header to:
     * - fix XWMA with buggy bit rates so FFmpeg can play them ok
     * - remove seek table to fix FFmpeg buggy XWMA seeking (see init_seek)
     * - read num_samples correctly
     */
#ifdef VGM_USE_FFMPEG
    {
        vgmstream->codec_data = init_ffmpeg_xwma(sf, xwma.data_offset, xwma.data_size, xwma.format, xwma.channels, xwma.sample_rate, xwma.avg_bitrate, xwma.block_size);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        /* try from (optional) seek table, or (less accurate) manual count */
        vgmstream->num_samples = xwma_dpds_get_samples(sf, xwma.dpds_offset, xwma.dpds_size, xwma.channels, 0);
        if (!vgmstream->num_samples)
            vgmstream->num_samples = xwma_get_samples(sf, xwma.data_offset, xwma.data_size, xwma.format, xwma.channels, xwma.sample_rate, xwma.block_size);
    }
#else
    goto fail;
#endif

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
