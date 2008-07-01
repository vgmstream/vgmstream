#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/* GENH is an artificial "generic" header for headerless streams */

VGMSTREAM * init_vgmstream_genh(STREAMFILE *streamFile) {
    
	VGMSTREAM * vgmstream = NULL;
    
	int32_t channel_count;
    int32_t interleave;
    int32_t sample_rate;
    int32_t loop_start;
    int32_t loop_end;
    int32_t start_offset;
    int32_t header_size;
    char filename[260];
    int coding;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("genh",filename_extension(filename))) goto fail;

    /* check header magic */
    if (read_32bitBE(0x0,streamFile) != 0x47454e48) goto fail;

    /* check channel count (needed for ADP/DTK check) */
    channel_count = read_32bitLE(0x4,streamFile);
    if (channel_count < 1) goto fail;

    /* check format */
    /* 0 = PSX ADPCM */
    /* 1 = XBOX IMA ADPCM */
    /* 2 = NGC ADP/DTK ADPCM */
    /* 3 = 16bit big endian PCM */
    /* 4 = 16bit little endian PCM */
    /* 5 - 8bit PCM */
    /* 6 - SDX2 */
    /* ... others to come */
    switch (read_32bitLE(0x18,streamFile)) {
        case 0:
            coding = coding_PSX;
            break;
        case 1:
            coding = coding_XBOX;
            break;
        case 2:
            coding = coding_NGC_DTK;
            if (channel_count != 2) goto fail;
            break;
        case 3:
            coding = coding_PCM16BE;
            break;
        case 4:
            coding = coding_PCM16LE;
            break;
        case 5:
            coding = coding_PCM8;
            break;
        case 6:
            coding = coding_SDX2;
            break;
        default:
            goto fail;
    }

    start_offset = read_32bitLE(0x1c,streamFile);
    header_size = read_32bitLE(0x20,streamFile);

    /* HACK to support old genh */
    if (header_size == 0) {
        start_offset = 0x800;
        header_size = 0x800;
    }

    /* check for audio data start past header end */
    if (header_size > start_offset) goto fail;

    interleave = read_32bitLE(0x8,streamFile);
    sample_rate = read_32bitLE(0xc,streamFile);
    loop_start = read_32bitLE(0x10,streamFile);
    loop_end = read_32bitLE(0x14,streamFile);

    if (coding == coding_XBOX && channel_count != 2) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,(loop_start!=-1));
    if (!vgmstream) goto fail;

    /* fill in the vital information */

    vgmstream->channels = channel_count;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = loop_end;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;
    vgmstream->loop_flag = (loop_start != -1);

	vgmstream->coding_type = coding;
    switch (coding) {
        case coding_PCM16LE:
        case coding_PCM16BE:
        case coding_PCM8:
        case coding_SDX2:
        case coding_PSX:
            vgmstream->interleave_block_size = interleave;
            if (channel_count > 1)
            {
                vgmstream->layout_type = layout_interleave;
            } else {
                vgmstream->layout_type = layout_none;
            }
            break;
        case coding_XBOX:
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 36;

            break;
        case coding_NGC_DTK:
            vgmstream->layout_type = layout_dtk_interleave;
            break;
    }
    
	vgmstream->meta_type = meta_GENH;
    
    /* open the file for reading by each channel */
    {
        int i;
        STREAMFILE * chstreamfile = NULL;

        for (i=0;i<channel_count;i++) {
            off_t chstart_offset = start_offset;

            switch (coding) {
                case coding_PSX:
                case coding_PCM16BE:
                case coding_PCM16LE:
                case coding_SDX2:
                case coding_PCM8:
                    if (vgmstream->layout_type == layout_interleave) {
                        if (interleave >= 512) {
                            chstreamfile =
                                streamFile->open(streamFile,filename,interleave);
                        } else {
                            if (!chstreamfile)
                                chstreamfile =
                                    streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
                        }
                        chstart_offset =
                            start_offset+vgmstream->interleave_block_size*i;
                    } else {
                        chstreamfile =
                            streamFile->open(streamFile,filename,
                                    STREAMFILE_DEFAULT_BUFFER_SIZE);
                    }
                    break;
                case coding_XBOX:
                    /* xbox's "interleave" is a lie, all channels start at same
                     * offset */
                    chstreamfile =
                        streamFile->open(streamFile,filename,
                                STREAMFILE_DEFAULT_BUFFER_SIZE);
                    break;
                case coding_NGC_DTK:
                    if (!chstreamfile) 
                        chstreamfile =
                            streamFile->open(streamFile,filename,32*0x400);
                    break;
            }

            if (!chstreamfile) goto fail;

            vgmstream->ch[i].streamfile = chstreamfile;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=chstart_offset;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
