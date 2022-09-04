#include "meta.h"
#include "../coding/coding.h"


/* VAGp - Sony SDK format, created by various official tools */
VGMSTREAM* init_vgmstream_vag(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t file_size, channel_size, interleave, interleave_first = 0, interleave_first_skip = 0;
    meta_t meta_type;
    int channels = 0, loop_flag, sample_rate;
    uint32_t vag_id, version, reserved;
    int32_t loop_start_sample = 0, loop_end_sample = 0;
    int allow_dual_stereo = 0, has_interleave_last = 0;


    /* checks */
    if (((read_u32be(0x00,sf) & 0xFFFFFF00) != get_id32be("VAG\0")) &&
        ((read_u32le(0x00,sf) & 0xFFFFFF00) != get_id32be("VAG\0")))
        goto fail;

    /* .vag: standard
     * .swag: Frantix (PSP)
     * .str: Ben10 Galactic Racing
     * .vig: MX vs. ATV Untamed (PS2)
     * .l/r: Crash Nitro Kart (PS2), Gradius V (PS2)
     * .vas: Kingdom Hearts II (PS2)
     * .xa2: Shikigami no Shiro (PS2)
     * .snd: Alien Breed (Vita) */
    if (!check_extensions(sf,"vag,swag,str,vig,l,r,vas,xa2,snd"))
        goto fail;

    file_size = get_streamfile_size(sf);

    /* versions used to create the file:
     * - 00000000 = v1.8 PC
     * - 00000002 = v1.3 Mac (used?)
     * - 00000003 = v1.6+ Mac
     * - 00000020 = v2.0 PC (most common)
     * - 00000004 = ? (later games)
     * - 00000006 = ? (vagconv)
     * - 00020001 = v2.1 (vagconv2)
     * - 00030000 = v3.0 (vagconv2) */

    vag_id = read_u32be(0x00,sf);
    version = read_u32be(0x04,sf);
    reserved = read_u32be(0x08,sf);
    channel_size = read_u32be(0x0c,sf);
    sample_rate = read_u32be(0x10,sf);
    /* 0x14-20 reserved */
    /* 0x20-30: name (optional) */
    /* 0x30: data start (first 0x10 usually 0s to init SPU) */

    /* check variation */
    switch(vag_id) {

        case 0x56414731: /* "VAG1" [Metal Gear Solid 3 (PS2), Cabela's African Safari (PSP), Shamu's Deep Sea Adventures (PS2)] */
            meta_type = meta_PS2_VAG1; //TODO not always Konami (Sand Grain Studios)
            start_offset = 0x40; /* 0x30 is extra data in VAG1 */
            interleave = 0x10;
            loop_flag = 0;

            /* MGS3 is 0 while Cabela's has this, plus description is 0x10 " " then 0x10 "-" */
            channels = read_u8(0x1e, sf);
            if (channels == 0)
                channels = 1;
            break;

        case 0x56414732: /* "VAG2" (2 channels) [Metal Gear Solid 3 (PS2)] */
            meta_type = meta_PS2_VAG2;
            start_offset = 0x40; /* 0x30 is extra data in VAG2 */
            channels = 2;
            interleave = 0x800;
            loop_flag = 0;
            break;

        case 0x56414769: /* "VAGi" (interleaved) */
            meta_type = meta_PS2_VAGi;
            start_offset = 0x800;
            channels = 2;
            interleave = read_u32le(0x08,sf);
            loop_flag = 0;
            break;

        case 0x70474156: /* pGAV (little endian / stereo) [Jak 3 (PS2), Jak X (PS2)] */
            meta_type = meta_PS2_pGAV;
            start_offset = 0x30;

            if (is_id32be(0x20,sf, "Ster")) {
                channels = 2;

                if (is_id32be(0x2000,sf, "pGAV"))
                    interleave = 0x2000; /* Jak 3 interleave, includes header */
                else if (is_id32be(0x1000,sf, "pGAV"))
                    interleave = 0x1000; /* Jak X interleave, includes header */
                else
                    interleave = 0x2000; /* Jak 3 interleave in rare files, no header */
                interleave_first = interleave - start_offset; /* interleave includes header */
                interleave_first_skip = start_offset;
            }
            else {
                channels = 1;
                interleave = 0;
            }

            channel_size = read_u32le(0x0C,sf) / channels;
            sample_rate = read_s32le(0x10,sf);
            //todo adjust channel_size, includes part of header?
            loop_flag = 0;
            break;

        case 0x56414770: /* "VAGp" (standard and variations) */
            meta_type = meta_PS2_VAGp;

            if (check_extensions(sf,"vig")) {
                /* MX vs. ATV Untamed (PS2) */
                start_offset = 0x800 - 0x20;
                channels = 2;
                interleave = 0x10;
                loop_flag = 0;
            }
            else if (check_extensions(sf,"swag")) { /* also "VAGp" at (file_size / channels) */
                /* Frantix (PSP) */
                start_offset = 0x40; /* channel_size ignores empty frame */
                channels = 2;
                interleave = file_size / channels;

                channel_size = read_u32le(0x0c,sf);
                sample_rate = read_s32le(0x10,sf);

                loop_flag = ps_find_loop_offsets(sf, start_offset, channel_size*channels, channels, interleave, &loop_start_sample, &loop_end_sample);
            }
            else if (is_id32be(0x6000,sf, "VAGp")) {
                /* The Simpsons Wrestling (PS1) */
                start_offset = 0x30;
                channels = 2;
                interleave = 0x6000;
                interleave_first = interleave - start_offset; /* includes header */
                interleave_first_skip = start_offset;

                loop_flag = 0;
            }
            else if (is_id32be(0x1000,sf, "VAGp")) {
                /* Shikigami no Shiro (PS2) */
                start_offset = 0x30;
                channels = 2;
                interleave = 0x1000;
                interleave_first = interleave - start_offset; /* includes header */
                interleave_first_skip = start_offset;

                loop_flag = ps_find_loop_offsets(sf, start_offset, channel_size*channels, channels, interleave, &loop_start_sample, &loop_end_sample);
            }
            else if (version == 0x40000000) {
                /* Killzone (PS2) */
                start_offset = 0x30;
                channels = 1;
                interleave = 0;

                channel_size = read_u32le(0x0C,sf) / channels;
                sample_rate = read_s32le(0x10,sf);
                loop_flag = 0;
            }
            else if (version == 0x00020001 || version == 0x00030000) {
                /* standard Vita/PS4 .vag [Chronovolt (Vita), Grand Kingdom (PS4)] */
                start_offset = 0x30;
                interleave = 0x10;

                /* channels are at 0x1e, except Ukiyo no Roushi (Vita), which has
                 * loop start/end frame (but also uses PS-ADPCM flags) */
                if (read_u32be(0x18,sf) == 0
                        && (read_u32be(0x1c,sf) & 0xFFFF00FF) == 0
                        && read_u8(0x1e,sf) < 16) {
                    channels = read_u8(0x1e,sf);
                    if (channels == 0)
                        channels = 1;  /* ex. early games [Lumines (Vita)] */
                }
                else {
                    channels = 1;
                }

                channel_size = channel_size / channels;
                loop_flag = ps_find_loop_offsets(sf, start_offset, channel_size*channels, channels, interleave, &loop_start_sample, &loop_end_sample);
            }
            else if (version == 0x00000004 && channel_size == file_size - 0x60 && read_u32be(0x1c, sf) != 0) { /* also .vas */
                /* Kingdom Hearts II (PS2) */
                start_offset = 0x60;
                interleave = 0x10;

                loop_start_sample = read_s32be(0x14,sf);
                loop_end_sample = read_s32be(0x18,sf);
                loop_flag = (loop_end_sample > 0); /* maybe at 0x1d */
                channels = read_u8(0x1e,sf);
                /* 0x1f: possibly volume */
                channel_size = channel_size / channels;
                /* mono files also have channel/volume, but start at 0x30 and are probably named .vag */
            }
            else if (version == 0x00000020 && is_id32be(0x30,sf, "STER") && is_id32be(0x34,sf, "EOVA") && is_id32be(0x38,sf, "G2K\0")) {
                /* The Simpsons Skateboarding (PS2) */
                start_offset = 0x800;
                channels = 2;
                interleave = 0x800;
                loop_flag = 0;
            }
            else if (version == 0x00000002 && is_id32be(0x24, sf, "VAGx")) {
                /* Need for Speed: Hot Pursuit 2 (PS2) */
                start_offset = 0x30;
                channels = read_u32be(0x2c, sf);
                channel_size = channel_size / channels;
                loop_flag = 0;

                if (file_size % 0x10 != 0)
                    goto fail;

                /* detect interleave using end markers */
                interleave = 0;

                if (channels > 1) {
                    off_t offset = file_size;
                    off_t end_off = 0;
                    uint8_t flag;

                    while (offset > start_offset) {
                        offset -= 0x10;
                        flag = read_u8(offset + 0x01, sf);
                        if (flag == 0x01) {
                            if (!end_off) {
                                end_off = offset;
                            } else {
                                interleave = end_off - offset;
                                break;
                            }
                        }
                    }

                    if (!interleave) goto fail;
                }
            }
            else if (version == 0x00000020 && channel_size == file_size - 0x800 && read_u32be(0x08, sf) == 0x01) {
                /* Garfield: Saving Arlene (PS2) */
                start_offset = 0x800;
                channels = 2;
                interleave = 0x400;
                loop_flag = 0;

                channel_size -= ps_find_padding(sf, start_offset, channel_size, channels, interleave, 0);
                channel_size = channel_size / channels;
            }
            else if (version == 0x00000020 && reserved == 0x01010101) {
                /* Gift (PS2) */
                start_offset = 0x800;
                channels = 2; /* mono VAGs in this game are standard, without reserved value */ 
                interleave = 0x2000;
                if (read_u32be(0x4800,sf) == 0x00000000) /* one file has bigger interleave, detectable with ch2's null frame */
                    interleave = 0x4000;

                channel_size = channel_size / channels;
                has_interleave_last = 1;

                /* all files do full loops */
                loop_flag = 1;
                loop_start_sample = 0;
                loop_end_sample = ps_bytes_to_samples(channel_size,1);
            }
            else {
                /* standard PS1/PS2/PS3 .vag [Ecco the Dolphin (PS2), Legasista (PS3)] */
                start_offset = 0x30;
                interleave = 0;

                channels = 1;
                if (version == 0x20 /* hack for repeating full loops that aren't too small */
                        && ps_bytes_to_samples(channel_size, 1) > 20 * sample_rate) {
                    loop_flag = ps_find_loop_offsets_full(sf, start_offset, channel_size*channels, channels, interleave, &loop_start_sample, &loop_end_sample);
                }
                else {
                    loop_flag = ps_find_loop_offsets(sf, start_offset, channel_size*channels, channels, interleave, &loop_start_sample, &loop_end_sample);
                }
                allow_dual_stereo = 1; /* often found with external L/R files */
            }
            break;

        default:
            goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_type;
    vgmstream->allow_dual_stereo = allow_dual_stereo;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(channel_size,1);
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;
    vgmstream->coding_type = coding_PSX;
    if (version == 0x00020001 || version == 0x00030000)
        vgmstream->coding_type = coding_HEVAG;
    vgmstream->layout_type = (channels == 1) ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = interleave;
    vgmstream->interleave_first_block_size = interleave_first;
    vgmstream->interleave_first_skip = interleave_first_skip;
    if (has_interleave_last && channels > 1 && interleave)
        vgmstream->interleave_last_block_size = channel_size % interleave;

    read_string(vgmstream->stream_name,0x10+1, 0x20,sf); /* always, can be null */

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* AAAp - Acclaim Austin Audio VAG header [The Red Star (PS2)] */
VGMSTREAM* init_vgmstream_vag_aaap(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t vag_offset, start_offset;
    uint32_t channel_size, sample_rate;
    uint16_t interleave, channels;
    uint32_t i;
    int loop_flag;

    /* checks */
    /* .vag - assumed, we don't know the original filenames */
    if (!check_extensions(sf, "vag"))
        goto fail;

    if (read_u32be(0x00, sf) != 0x41414170) /* "AAAp" */
        goto fail;

    interleave = read_u16le(0x04, sf);
    channels = read_u16le(0x06, sf);
    vag_offset = 0x08;

    /* file has VAGp header for each channel */
    for (i = 0; i < channels; i++) {
        if (read_u32be(vag_offset + i * 0x30, sf) != 0x56414770) /* "VAGp" */
            goto fail;
    }
    
    /* check version */
    if (read_u32be(vag_offset + 0x04, sf) != 0x00000020)
        goto fail;

    channel_size = read_u32be(vag_offset + 0x0c, sf);
    sample_rate = read_u32be(vag_offset + 0x10, sf);
    start_offset = vag_offset + channels * 0x30;
    loop_flag = 0;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PS2_VAGp_AAAP;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(channel_size, 1);
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
