#include "meta.h"
#include "../coding/coding.h"

typedef enum { PSX, DSP, IMA, PCM } vas_codec_t;

/* .VAS - from Konami Computer Enterntainment Osaka games [Jikkyou Powerful Pro Yakyuu 8 (PS2), TMNT 2: Battle Nexus (multi)] */
VGMSTREAM* init_vgmstream_vas_kceo(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, block_size, sample_rate, volume, dummy;
    uint32_t loop_start, loop_end, data_size;
    vas_codec_t codec;


    /* checks */
    /* .vas: bigfile extension (internally files just seem to be referred as 'bgm' or 'stream')
     * .dsp: assumed (GC bigfile is just .bin and has no table) */
    if (!check_extensions(sf, "vas,dsp"))
        return NULL;

    if (read_u32le(0x00, sf) == 0x01) { /* PC */
        channels    = read_u32le(0x04, sf);
        block_size  = read_u32le(0x08, sf);
        sample_rate = read_s32le(0x0c, sf);
        loop_start  = read_u32le(0x10, sf);
        loop_end    = read_u32le(0x14, sf);
        volume      = read_u32le(0x18, sf);
        loop_flag   = read_u32le(0x1c, sf) != 0; // unknown size (low-ish), 0 if no loop
        // 20: 0x8000 if loop?
        data_size   = read_u32le(0x24, sf);
        // 28: null?
        // 2c: null?
        dummy       = read_u32le(0x30, sf);

        if (block_size != channels * 0x02)
            return NULL;
        codec = PCM;
    }
    else if (read_u32le(0x00, sf) == 0x69) { /* Xbox */
        channels    = read_u32le(0x04, sf);
        block_size  = read_u32le(0x08, sf);
        sample_rate = read_s32le(0x0c, sf);
        loop_start  = read_u32le(0x10, sf);
        loop_end    = read_u32le(0x14, sf);
        volume      = read_u32le(0x18, sf);
        loop_flag   = read_u32le(0x1c, sf) != 0; // unknown size (low-ish), 0 if no loop
        // 20: 0x8000 if loop?
        data_size   = read_u32le(0x24, sf);
        // 28: null or some kind of hash (TMNT3)
        // 2c: null or codec + channels (TMNT3)
        dummy       = read_u32le(0x30, sf);

        if (block_size != channels * 0x24)
            return NULL;
        codec = IMA;
    }
    else if (read_u32le(0x00,sf) + 0x800 == get_streamfile_size(sf)) { /* PS2 */
        data_size   = read_u32le(0x00, sf);
        sample_rate = read_s32le(0x04,sf);
        volume      = read_u32le(0x08, sf);
        dummy       = read_u32le(0x0c, sf);
        loop_flag   = read_u32le(0x10, sf) != 0; // 0/1
        loop_start  = read_u32le(0x14,sf);

        channels = 2;
        codec = PSX;

        loop_end = data_size; 

        if (!ps_check_format(sf, 0x800, 0x1000))
            return NULL;
    }
    else if (read_u32be(0x00,sf) + 0x800 == get_streamfile_size(sf)) { /* GC */
        data_size   = read_u32be(0x00, sf);
        sample_rate = read_s32be(0x04,sf);
        volume      = read_u32be(0x08, sf);
        dummy       = read_u32be(0x0c, sf);
        loop_flag   = read_u32be(0x10, sf) != 0; // 0/1
        loop_start  = read_u32be(0x14,sf);

        channels = 2;
        codec = DSP;

        loop_end = data_size; 

        // DSP header variation at 0x80 (loop_end seems smaller but not correct?)
        if (read_u32be(0x8c, sf) != 0x0002) // codec
            return NULL;
    }
    else {
        return NULL;
    }

    /* simple header so do a few extra checks */
    if (channels != 2) // voices have a slightly different, simpler format
        return NULL;
    if (sample_rate > 48000 || sample_rate < 8000)
        return NULL;
    if (volume <= 0x00 || volume > 0xFF) /* typically 0x96, some PS2 use ~0xC0  */
        return NULL;
    if (dummy != 0)
        return NULL;

    start_offset = 0x800;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VAS_KCEO;
    vgmstream->sample_rate = sample_rate;

    switch (codec) {
        case PCM:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            vgmstream->num_samples = pcm16_bytes_to_samples(data_size, vgmstream->channels);  
            vgmstream->loop_start_sample = pcm16_bytes_to_samples(loop_start, vgmstream->channels);
            vgmstream->loop_end_sample = pcm16_bytes_to_samples(loop_end, vgmstream->channels);
            break;

        case IMA:
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_blocked_vas_kceo;

            // blocks of 0x20000 with 0x20 padding, remove it to calculate samples
            data_size -= (data_size / 0x20000) * 0x20;
            loop_start -= (loop_start / 0x20000) * 0x20;
            loop_end -= (loop_end / 0x20000) * 0x20;

            vgmstream->num_samples = xbox_ima_bytes_to_samples(data_size, vgmstream->channels);  
            vgmstream->loop_start_sample = xbox_ima_bytes_to_samples(loop_start, vgmstream->channels);
            vgmstream->loop_end_sample = xbox_ima_bytes_to_samples(loop_end, vgmstream->channels);
            break;

        case PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x200;

            vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);
            vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, channels);
            vgmstream->loop_end_sample = ps_bytes_to_samples(loop_end, channels);
            break;

        case DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x100;

            vgmstream->num_samples = dsp_bytes_to_samples(data_size, channels);
            vgmstream->loop_start_sample = dsp_bytes_to_samples(loop_start, channels);
            vgmstream->loop_end_sample = dsp_bytes_to_samples(loop_end, channels);

            dsp_read_coefs_be(vgmstream, sf, 0x90, 0x40);
            break;

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* .VAS in containers */
VGMSTREAM* init_vgmstream_vas_kceo_container(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t subfile_offset = 0;
    uint32_t subfile_size = 0;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!check_extensions(sf, "vas"))
        return NULL;

    if (read_u32be(0x00, sf) == 0xAB8A5A00) { /* PS2 (fixed value) */

        /* unknown size/id */
        if (read_u32le(0x04, sf) * 0x800 + 0x800 != get_streamfile_size(sf))
            return NULL;

        total_subsongs = read_s32le(0x08, sf); /* also at 0x10 */
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        if (total_subsongs > 0x100) /* arbitrary max */
            return NULL;

        /* check offset table flag, 0x98 has table size */
        if (read_u32le(0x94, sf)) {
            off_t header_offset = 0x800 + 0x10*(target_subsong-1);

            /* some values are repeats found in the file sub-header */
            subfile_offset = read_u32le(header_offset + 0x00,sf) * 0x800;
            subfile_size   = read_u32le(header_offset + 0x08,sf) + 0x800;
        }
        else {
            /* a bunch of files */
            off_t offset = 0x800;
            int i;

            for (i = 0; i < total_subsongs; i++) {
                size_t size = read_u32le(offset, sf) + 0x800;

                if (i + 1 == target_subsong) {
                    subfile_offset = offset;
                    subfile_size = size;
                    break;
                }

                offset += size;
            }
            if (i == total_subsongs)
                return NULL;
        }
    }
    else if (read_u32le(0x00, sf) == 0x800) { /* Xbox/PC (start?) */
        total_subsongs = read_s32le(0x04, sf);
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        if (total_subsongs > 0x100) /* arbitrary max */
            return NULL;
        if (read_u32le(0x08, sf) != 0x800)
            return NULL;

        /* table of offset + ? size */
        uint32_t header_offset = 0x08 + 0x08 * (target_subsong - 1);
        subfile_offset = read_u32le(header_offset + 0x00,sf);
        uint32_t next_offset = (target_subsong == total_subsongs) ? 
            get_streamfile_size(sf) :
            read_u32le(header_offset + 0x08,sf);
        subfile_size   = next_offset - subfile_offset;
    }
    else {
        /* some .vas are just files pasted together, better extracted externally but whatevs */
        uint32_t file_size = get_streamfile_size(sf);
        uint32_t offset = 0;

        /* must have multiple .vas */
        if (read_u32le(0x00,sf) + 0x800 >= file_size)
           goto fail;

        total_subsongs = 0;
        if (target_subsong == 0) target_subsong = 1;

        while (offset < file_size) {
            uint32_t size = read_u32le(offset,sf) + 0x800;

            /* some files can be null, ignore */
            if (size > 0x800) {
                total_subsongs++;

                if (total_subsongs == target_subsong) {
                    subfile_offset = offset;
                    subfile_size = size;
                }
            }

            offset += size;
        }

        /* should end exactly at file_size */
        if (offset > file_size)
            goto fail;

        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
    }


    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, NULL);
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_vas_kceo(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_subsongs;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
