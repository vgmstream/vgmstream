#include <math.h>
#include "adx.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_adx(const char * const filename) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;
    off_t copyright_offset;
    size_t filesize;
    uint32_t version_signature;
    int loop_flag=0;
    int channel_count;
    int32_t loop_start_offset;
    int32_t loop_end_offset;
    int32_t loop_start_sample;
    int32_t loop_end_sample;
    meta_t header_type;
    int16_t coef1, coef2;

    /* check extension, case insensitive */
    if (strcasecmp("adx",filename_extension(filename))) goto fail;

    /* try to open the file for header reading */
    infile = open_streamfile(filename);
    if (!infile) goto fail;

    filesize = get_streamfile_size(infile);

    /* check first 2 bytes */
    if ((uint16_t)read_16bitBE(0,infile)!=0x8000) goto fail;

    /* get copyright/stream offset, check */
    copyright_offset = read_16bitBE(2,infile);
    if ((uint32_t)read_32bitBE(copyright_offset,infile)!=0x29435249 || /* ")CRI" */
            (uint16_t)read_16bitBE(copyright_offset-2,infile)!=0x2863) /* "(c" */
        goto fail;

    /* check version signature, read loop info */
    /* TODO: at some point will we want to support encrypted ADXs? */
    version_signature = read_32bitBE(0x10,infile);
    if (version_signature == 0x01F40300) {      /* type 03 */
        header_type = meta_ADX_03;
        if (copyright_offset-2 >= 0x2c) {   /* enough space for loop info? */
            loop_flag = (read_32bitBE(0x18,infile) != 0);
            loop_start_sample = read_32bitBE(0x1c,infile);
            loop_start_offset = read_32bitBE(0x20,infile);
            loop_end_sample = read_32bitBE(0x24,infile);
            loop_end_offset = read_32bitBE(0x28,infile);
        }
    } else if (version_signature == 0x01F40400) {
        header_type = meta_ADX_04;
        if (copyright_offset-2 >= 0x38) {   /* enough space for loop info? */
            loop_flag = (read_32bitBE(0x24,infile) != 0);
            loop_start_sample = read_32bitBE(0x28,infile);
            loop_start_offset = read_32bitBE(0x2c,infile);
            loop_end_sample = read_32bitBE(0x30,infile);
            loop_end_offset = read_32bitBE(0x34,infile);
        }
    } else goto fail;   /* not a known/supported version signature */

    /* At this point we almost certainly have an ADX file,
     * so let's build the VGMSTREAM. */

    channel_count = read_8bit(7,infile);
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = read_32bitBE(0xc,infile);
    vgmstream->sample_rate = read_32bitBE(8,infile);
    /* channels and loop flag are set by allocate_vgmstream */
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;

    vgmstream->coding_type = coding_CRI_ADX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = header_type;

    vgmstream->interleave_block_size=18;


    close_streamfile(infile); infile=NULL;

    /* calculate filter coefficients */
    {
        double x,y,z,a,b,c;

        x = 500;
        y = vgmstream->sample_rate;
        z = cos(2.0*M_PI*x/y);

        a = M_SQRT2-z;
        b = M_SQRT2-1.0;
        c = (a-sqrt((a+b)*(a-b)))/b;

        coef1 = floor(c*8192);
        coef2 = floor(c*c*-4096);
    }

    {
        int i;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = open_streamfile_buffer(filename,18*0x400);
            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=
                copyright_offset+4+18*i;

            vgmstream->ch[i].adpcm_coef[0] = coef1;
            vgmstream->ch[i].adpcm_coef[1] = coef2;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (infile) close_streamfile(infile);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

void decode_adx(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i=first_sample;
    int32_t sample_count;

    int32_t scale = read_16bitBE(stream->offset,stream->streamfile) + 1;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;
    int coef1 = stream->adpcm_coef[0];
    int coef2 = stream->adpcm_coef[1];

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int sample_byte = read_8bit(stream->offset+2+i/2,stream->streamfile);

        outbuf[sample_count] = clamp16(
                (i&1?
                 get_low_nibble_signed(sample_byte):
                 get_high_nibble_signed(sample_byte)
                ) * scale +
                ((coef1 * hist1 + coef2 * hist2) >> 12)
                );

        hist2 = hist1;
        hist1 = outbuf[sample_count];
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}
