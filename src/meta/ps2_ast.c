#include "meta.h"
#include "../coding/coding.h"

/* AST - from Koei and Marvelous games (same internal dev?) */
VGMSTREAM * init_vgmstream_ps2_ast(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, variant_type;

    /* check extension */
    if (!check_extensions(streamFile,"ast")) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x41535400) /* "AST\0" */
        goto fail;

    /* determine variant (after 0x10 is garbage/code data in type 1 until 0x800, but consistent in all songs) */
    if (read_32bitBE(0x10,streamFile) == 0x00000000 || read_32bitBE(0x10,streamFile) == 0x20002000) {
        variant_type = 1; /* Koei: P.T.O. IV (0x00000000), Naval Ops: Warship Gunner (0x20002000) */
        channel_count = 2;
    }
    else {
        variant_type = 2; /* Marvelous: Katekyoo Hitman Reborn! Dream Hyper Battle!, Binchou-tan: Shiawasegoyomi */
        channel_count = read_32bitLE(0x0C,streamFile);
    }

    loop_flag = 0;    

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    if (variant_type == 1) {
        start_offset = 0x800;
        vgmstream->sample_rate = read_32bitLE(0x04,streamFile);
        vgmstream->num_samples = ps_bytes_to_samples(read_32bitLE(0x0C,streamFile)-start_offset,channel_count);
        vgmstream->interleave_block_size = read_32bitLE(0x08,streamFile);
    }
    else if (variant_type == 2) {
        start_offset = 0x100;
        vgmstream->sample_rate = read_32bitLE(0x08,streamFile);
        vgmstream->num_samples = ps_bytes_to_samples(read_32bitLE(0x04,streamFile)-start_offset,channel_count);
        vgmstream->interleave_block_size = read_32bitLE(0x10,streamFile);
    }
    else {
        goto fail;
    }

    vgmstream->layout_type = layout_interleave;    
    vgmstream->coding_type = coding_PSX;
    vgmstream->meta_type = meta_PS2_AST;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
