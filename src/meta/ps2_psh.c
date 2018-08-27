#include "meta.h"
#include "../coding/coding.h"
#include "ps2_psh_streamfile.h"


/* PSH/VSV - from Square Enix games [Dawn of Mana: Seiken Densetsu 4 (PS2), Kingdom Hearts Re:Chain of Memories (PS2)] */
VGMSTREAM * init_vgmstream_ps2_psh(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t start_offset;
	int loop_flag, channel_count;
    size_t loop_start, adjust, data_size, interleave;


    /* checks */
    /* .psh: assumed? [Romancing SaGa: Minstrel's Song (PS2)]
     * .vsv: official? [Kingdom Hearts HD I.5 + II.5 ReMIX (PS4)] */
    if (!check_extensions(streamFile, "psh,vsv"))
        goto fail;
    /* 0x00(2): 0x0000 (RS:MS) / 0x6440 (KH:RCoM) / varies (DoM) */
    if ((uint16_t)read_16bitBE(0x02,streamFile) != 0x6400)
        goto fail;

    channel_count = 2;
    start_offset = 0x00; /* correct, but needs some tricks to fix sound (see below) */
    interleave = 0x800;

    adjust = (uint16_t)read_16bitLE(0x04,streamFile) & 0x7FF;  /* upper bits = ??? */
    data_size = (uint16_t)read_16bitLE(0x0c,streamFile) * interleave;
    /* 0x0e: ? (may be 0x0001, or a low-ish value, not related to looping?) */
    loop_start = ((uint16_t)read_16bitLE(0x06,streamFile) & 0x7FFF) * interleave; /* uper bit == loop flag? */
    loop_flag = (loop_start != 0); /* (no known files loop from beginning to end) */


	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PS2_PSH;
	vgmstream->sample_rate = (uint16_t)read_16bitLE(0x08,streamFile);
	vgmstream->num_samples = ps_bytes_to_samples(data_size,channel_count);

	vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start,channel_count);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    /* loops are odd, but comparing the audio wave with the OSTs these values seem correct */
    if (adjust == 0) { /* Romancing SaGa (PS2) */
        vgmstream->loop_start_sample -= ps_bytes_to_samples(channel_count*interleave,channel_count); /* maybe *before* loop block? */
        vgmstream->loop_start_sample -= ps_bytes_to_samples(0x200*channel_count,channel_count); /* maybe default adjust? */
    }
    else { /* all others */
        vgmstream->loop_end_sample -= ps_bytes_to_samples((0x800 - adjust)*channel_count,channel_count); /* at last block + adjust is a 0x03 flag */
    }

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    temp_streamFile = setup_ps2_psh_streamfile(streamFile, start_offset, data_size);
    if (!temp_streamFile) goto fail;

    if (!vgmstream_open_stream(vgmstream, temp_streamFile, start_offset))
        goto fail;

    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
