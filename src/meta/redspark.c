#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

static uint32_t rotlwi(uint32_t x, uint32_t r) {
        return (x << r) | (x >> (32-r));
}

static uint32_t find_key(uint32_t firstword) {
    uint32_t expected = 0x52656453;
    return firstword ^ expected;
}

/* RSD - RedSpark (MadWorld) 
   RS3D - RedSpark (Mario & Luigi: Dream Team I fi*/
VGMSTREAM * init_vgmstream_redspark(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;
    int loop_flag;
	int channel_count;
	int dt_flag = 0;
    uint32_t key;
    enum {encsize = 0x1000};
    uint8_t buf[encsize];
	int32_t(*get_32bit)(const uint8_t *p) = NULL;
	int16_t(*get_16bit)(const uint8_t *p) = NULL;
	get_16bit = get_16bitBE;
	get_32bit = get_32bitBE;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
	if (strcasecmp("rsd", filename_extension(filename))) goto fail;
    /* decrypt into buffer */
    {
        uint32_t data;
        int i;
        if (read_streamfile(buf,0,encsize,streamFile)!=encsize) goto fail;
		if (memcmp(&buf[0], "RedSpark", 8)) { //Check to see if already decrypted
			key = find_key(get_32bitBE(&buf[0]));
			data = get_32bitBE(&buf[0]) ^ key;
			put_32bitBE(&buf[0], data);
			key = rotlwi(key, 11);

			for (i = 4; i < encsize; i += 4) {
				key = rotlwi(key, 3) + key;
				data = get_32bitBE(&buf[i]) ^ key;
				put_32bitBE(&buf[i], data);
			}
		}
		else {
			get_16bit = get_16bitLE;
			get_32bit = get_32bitLE;
			dt_flag = 1;

			for (i = 4; i < encsize; i += 4) {
				data = get_32bitBE(&buf[i]);
				put_32bitBE(&buf[i], data);
			}
		}
    }

    /* check header */
    if (memcmp(&buf[0],"RedSpark",8))
        goto fail;

    loop_flag = (buf[0x4f] != 0);
    channel_count = buf[0x4e];

    /* make sure loop info is the only two cue points */
    if (loop_flag && buf[0x4f] != 2) goto fail;

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x1000;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = get_32bit(&buf[0x3c]);
    vgmstream->coding_type = coding_NGC_DSP;
	if (dt_flag)
		vgmstream->num_samples = get_32bit(&buf[0x40]);
	else
		vgmstream->num_samples = get_32bit(&buf[0x40])*14;
    if (loop_flag) {
        off_t start = 0x54;
        start += channel_count*8;
		if (dt_flag) {
        vgmstream->loop_start_sample = get_32bit(&buf[start+4]);
        vgmstream->loop_end_sample = (get_32bit(&buf[start+0xc]));
		}
		else {
			vgmstream->loop_start_sample = get_32bit(&buf[start+4])*14;
			vgmstream->loop_end_sample = (get_32bit(&buf[start+0xc])+1)*14;
		}
        if (vgmstream->loop_end_sample > vgmstream->num_samples) {
            vgmstream->loop_end_sample = vgmstream->num_samples;
        }
    }

    if (channel_count >= 2) {
        vgmstream->layout_type = layout_interleave;
        vgmstream->interleave_block_size = 8;
    } else {
        vgmstream->layout_type = layout_none;
    }
    vgmstream->meta_type = meta_REDSPARK;


    {
        off_t start = 0x54;
        int i,j;

        start += channel_count * 8;
        if (loop_flag) {
            start += 16;
        }

        for (j = 0; j < channel_count; j++) {
            for (i=0;i<16;i++) {
                vgmstream->ch[j].adpcm_coef[i] =
                    get_16bit(&buf[start+0x2e*j+i*2]);
            }
        }
    }

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=start_offset + i*vgmstream->interleave_block_size;

        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
