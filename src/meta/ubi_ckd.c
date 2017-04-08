#include "meta.h"
#include "../coding/coding.h"


/* CKD RIFF - Ubisoft audio [Rayman Origins (Wii)] */
VGMSTREAM * init_vgmstream_ubi_ckd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, first_offset = 0xc, chunk_offset;
    size_t chunk_size, data_size;
	int loop_flag, channel_count, interleave;


    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"ckd")) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52494646) /* RIFF */
		goto fail;
	if (read_32bitBE(0x26,streamFile) != 0x6473704C) /* dspL */
        goto fail;

    loop_flag = 0;
    channel_count = read_16bitBE(0x16,streamFile);
    
    /* find data chunk, in 3 variants */
    if (find_chunk_be(streamFile, 0x64617453,first_offset,0, &chunk_offset,&chunk_size)) { /*"datS"*/
        /* normal interleave */
        start_offset = chunk_offset;
        data_size = chunk_size;
        interleave = 8;
    } else if (find_chunk_be(streamFile, 0x6461744C,first_offset,0, &chunk_offset,&chunk_size)) { /*"datL"*/
        /* mono or full interleave (with a "datR" after the "datL", no check as we can just pretend it exists) */
        start_offset = chunk_offset;
        data_size = chunk_size * channel_count;
        interleave = (4+4) + chunk_size; /* don't forget to skip the "datR"+size chunk */
    } else {
        goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0x18,streamFile);
    vgmstream->num_samples = dsp_bytes_to_samples(data_size, channel_count);

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = channel_count==1 ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = interleave;
    vgmstream->meta_type = meta_UBI_CKD;

    dsp_read_coefs_be(vgmstream,streamFile, 0x4A, (4+4)+0x60);


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

