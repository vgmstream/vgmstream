#include "meta.h"
#include "../coding/coding.h"


/* VAGp - Sony SDK format, created by various official tools */
VGMSTREAM* init_vgmstream_vag(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, file_size, channel_size, stream_name_size, interleave, interleave_first = 0, interleave_first_skip = 0;
    meta_t meta_type;
    int channels = 0, loop_flag, sample_rate;
    uint32_t vag_id, version, reserved;
    int32_t loop_start_sample = 0, loop_end_sample = 0;
    int allow_dual_stereo = 0, has_interleave_last = 0;


    /* checks */
    if (((read_u32be(0x00,sf) & 0xFFFFFF00) != get_id32be("VAG\0")) &&
        ((read_u32le(0x00,sf) & 0xFFFFFF00) != get_id32be("VAG\0")))
        return NULL;

    /* .vag: standard
     * .swag: Frantix (PSP)
     * .str: Ben10 Galactic Racing
     * .vig: MX vs. ATV Untamed (PS2)
     * .l/r: Crash Nitro Kart (PS2), Gradius V (PS2)
     * .vas: Kingdom Hearts II (PS2)
     * .xa2: Shikigami no Shiro (PS2)
     * .snd: Alien Breed (Vita)
     * .svg: ModernGroove: Ministry of Sound Edition (PS2)
     * (extensionless): The Urbz (PS2), The Sims series (PS2)
     * .wav: Sniper Elite (PS2), The Simpsons Game (PS2/PSP) 
     * .msv: Casper and the Ghostly Trio (PS2), Earache Extreme Metal Racing (PS2) */
    if (!check_extensions(sf,"vag,swag,str,vig,l,r,vas,xa2,snd,svg,,wav,lwav,msv"))
        return NULL;

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

    /* a few Edge of Reality titles use the blank adpcm frame to
     * store a longer stream name, so the length is defined here
     * to allow for it to be overridden for such rare exceptions */
    stream_name_size = 0x10;

    /* check variation */
    switch(vag_id) {

        case 0x56414731: /* "VAG1" [Metal Gear Solid 3 (PS2), Cabela's African Safari (PSP), Shamu's Deep Sea Adventures (PS2)] */
            meta_type = meta_VAG_custom; //TODO not always Konami (Sand Grain Studios)
            start_offset = 0x40; /* 0x30 is extra data in VAG1 */
            interleave = 0x10;
            loop_flag = 0;

            /* MGS3 is 0 while Cabela's has this, plus description is 0x10 " " then 0x10 "-" */
            channels = read_u8(0x1e, sf);
            if (channels == 0)
                channels = 1;
            break;

        case 0x56414732: /* "VAG2" (2 channels) [Metal Gear Solid 3 (PS2)] */
            meta_type = meta_VAG_custom;
            start_offset = 0x40; /* 0x30 is extra data in VAG2 */
            channels = 2;
            interleave = 0x800;
            loop_flag = 0;
            break;

        case 0x56414769: /* "VAGi" (interleaved) */
            meta_type = meta_VAG_custom;
            start_offset = 0x800;
            channels = 2;
            interleave = read_u32le(0x08,sf);
            loop_flag = 0;
            break;

        case 0x70474156: /* pGAV (little endian / stereo) [Jak II, Jak 3, Jak X (PS2)] */
            meta_type = meta_VAG_custom;
            start_offset = 0x30;

            if (is_id32be(0x2000,sf, "pGAV"))
                interleave = 0x2000; /* Jak II & Jak 3 interleave, includes header */
            else if (is_id32be(0x1000,sf, "pGAV"))
                interleave = 0x1000; /* Jak X interleave, includes header */
            else
                interleave = 0;

            if (interleave) {
                channels = 2;
                interleave_first = interleave - start_offset; /* interleave includes header */
                interleave_first_skip = start_offset;
            }
            else {
                channels = 1;
            }

            channel_size = read_u32le(0x0C,sf) / channels;
            sample_rate = read_s32le(0x10,sf);
            //todo adjust channel_size, includes part of header?
            loop_flag = 0;
            break;

        case 0x56414770: /* "VAGp" (standard and variations) */
            meta_type = meta_VAG;

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
            else if (version == 0x00000020 && is_id32be(0x800,sf, "VAGp")) {
                /* ModernGroove: Ministry of Sound Edition (PS2) */
                start_offset = 0x30;
                channels = 2;
                interleave = 0x800;
                interleave_first = interleave - start_offset; /* includes header */
                interleave_first_skip = start_offset;

                loop_flag = 0;
            }
            else if (version == 0x02000000 || version == 0x40000000) {
                /* Edge of Reality engine (PS2) (0x02), Killzone (PS2) (0x40) */
                /* Stream starts at 0x40 for both variants. EoR/Maxis uses the
                 * blank SPU init frame to store the loop flag in its 1st byte.
                 * Later EoR games (Over the Hedge) have 32 char stream names,
                 * and moved the loop flag stored in the reserved field at 0x1E */
                start_offset = 0x40;
                channels = 1;
                interleave = 0;

                channel_size = read_u32le(0x0C,sf);
                sample_rate = read_s32le(0x10,sf);
                loop_flag = 0; /* adpcm flags always 0x02 in Killzone */

                /* EoR/Maxis title specific
                 * always blank in Killzone */
                if (version == 0x02000000) {
                    //uint8_t c = read_u8(0x30, sf);
                    /* maybe better to do (c >= 0x30 && c <= 0x7A)? */
                    if (read_u8(0x30, sf) >= 0x20 && read_u8(0x30, sf) <= 0x7E)
                        stream_name_size = 0x20;
                    loop_flag = ps_find_loop_offsets(sf, start_offset, channel_size, channels, interleave, &loop_start_sample, &loop_end_sample);
                }
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
                /* Eko Software */
                start_offset = 0x800;
                channels = 2; /* mono VAGs in this game are standard, without reserved value */

                /* detect interleave with ch2's null frame */
                if (read_u32be(0x800 + 0x400,sf) == 0x00000000) /* Woody Woodpecker: Escape from Buzz Buzzard Park (PS2) */
                    interleave = 0x400;
                else if (read_u32be(0x800 + 0x4000,sf) == 0x00000000) /* Gift (PS2), one file */
                    interleave = 0x4000;
                else if (read_u32be(0x800 + 0x2000,sf) == 0x00000000)  /* Gift (PS2) */
                    interleave = 0x2000;
                else
                    goto fail;

                channel_size = channel_size / channels;
                has_interleave_last = 1;

                /* all files do full loops */
                loop_flag = 1;
                loop_start_sample = 0;
                loop_end_sample = ps_bytes_to_samples(channel_size,1);
            }
            else if (version == 0x00000020 && channel_size == file_size + 0x10) {
                /* THQ Australia [Jimmy Neutron: Attack of the Twonkies, SpongeBob: Lights, Camera, Pants!] */
                start_offset = 0x30;
                interleave = 0;
                channels = 1;

                channel_size -= 0x40;
                loop_flag = ps_find_loop_offsets(sf, start_offset, channel_size, channels, interleave, &loop_start_sample, &loop_end_sample);
            }
            else if (version == 0x00000020 && is_id64be(0x20, sf, "KAudioDL") &&  ( (channel_size + 0x30) * 2 == file_size 
                || align_size(channel_size + 0x30, 0x800) * 2 == file_size ||  align_size(channel_size + 0x30, 0x400) * 2 == file_size) ) {
                /* .SKX stereo vag (name is always KAudioDLL and streams are padded unlike memory audio) [NBA 06 (PS2)] */
                start_offset = 0x30;
                interleave = file_size / 2;
                channels = 2; // mono KAudioDLL streams also exist

                loop_flag = ps_find_loop_offsets(sf, start_offset, channel_size, channels, interleave, &loop_start_sample, &loop_end_sample);
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

    /* ignore bigfiles and bad extractions (approximate) */
    /* padding is set to 2 MiB to avoid breaking Jak series' VAGs */
    if (channel_size * channels + interleave * channels + start_offset * channels + 0x200000 < file_size ||
        channel_size * channels > file_size) {
        vgm_logi("VAG: wrong expected (incorrect extraction? %x * %i + %x + %x + ~ vs %x)\n",
            channel_size, channels, interleave * channels, start_offset * channels, file_size);
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

    read_string(vgmstream->stream_name, stream_name_size + 1, 0x20, sf); /* always, can be null */

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
    if (!is_id32be(0x00, sf, "AAAp"))
        return NULL;

    /* .vag: original names before hashing */
    if (!check_extensions(sf, "vag"))
        return NULL;

    interleave = read_u16le(0x04, sf);
    channels = read_u16le(0x06, sf);
    vag_offset = 0x08;

    /* file has VAGp header for each channel */
    for (i = 0; i < channels; i++) {
        if (!is_id32be(vag_offset + i * 0x30, sf, "VAGp"))
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

    vgmstream->meta_type = meta_AAAP;
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


/* VAGp footer - sound data first, header at the end [The Sims 2: Pets (PS2), The Sims 2: Castaway (PS2)] */
VGMSTREAM* init_vgmstream_vag_footer(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    size_t file_size, stream_size;
    off_t header_offset, start_offset;
    int channels, interleave, sample_rate, loop_flag;
    int32_t loop_start_sample = 0, loop_end_sample = 0;
    uint32_t version;


    /* checks */
    /* check if this begins with valid PS-ADPCM */
    if (!ps_check_format(sf, 0x00, 0x40))
        return NULL;

    /* (extensionless): Sims 2 console spinoffs
     * .vag: assumed, may be added by tools */
    if (!check_extensions(sf, ",vag"))
        return NULL;

    file_size = get_streamfile_size(sf);
    header_offset = file_size - 0x40;

    if (!is_id32be(header_offset, sf, "VAGp"))
        return NULL;


    /* all the data is in little endian */
    version = read_u32le(header_offset + 0x04, sf);
    stream_size = read_u32le(header_offset + 0x0C, sf);
    sample_rate = read_u32le(header_offset + 0x10, sf);

    /* what's meant to be the SPU init frame instead has garbage data, apart from the very 1st byte */
    /* see the comment under (case 0x56414770:) where (version == 0x02000000) in init_vgmstream_vag */
    //loop_flag = read_u8(header_offset + 0x30, sf); /* ? */

    /* in the very unlikely chance anyone else was
     * unhinged enough to do something like this */
    if (version != 0x00000002) goto fail;
    /* stream "header" (footer) is aligned to 0x40 */
    if (align_size_to_block(stream_size + 0x40, 0x40) != file_size)
        goto fail;

    channels = 1;
    interleave = 0;
    start_offset = 0;

    loop_flag = ps_find_loop_offsets(sf, start_offset, stream_size, channels, interleave, &loop_start_sample, &loop_end_sample);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VAG_footer;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_none;
    vgmstream->sample_rate = sample_rate;
    vgmstream->interleave_block_size = interleave;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;
    vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);

    read_string(vgmstream->stream_name, 0x10 + 1, header_offset + 0x20, sf);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* .VAG - Evolution Games [Nickelodeon Rocket Power: Beach Bandits (PS2)] */
VGMSTREAM* init_vgmstream_vag_evolution_games(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    size_t stream_size;
    off_t start_offset;
    int channels, interleave, sample_rate, loop_flag;


    /* checks */
    if (!check_extensions(sf, "vag"))
        return NULL;

    /* VAGp replaced with 3 spaces + NUL */
    if (!is_id32be(0x00, sf, "   \0"))
        return NULL;


    /* all the data is in little endian */
    if (read_u32le(0x04, sf) != 0) goto fail; /* version */
    if (!is_id32be(0x08, sf, "   \0")) goto fail; /* reserved */
    stream_size = read_u32le(0x0C, sf);
    sample_rate = read_u32le(0x10, sf);
    /* reserved 0x14 == "    "
     * reserved 0x18 == "    "
     * reserved 0x1C == "   \0"
     */
    /* starting to think the padding was made with null-terminated strings */

    /* data is often aligned to 0x80, but not always */
    if (stream_size + 0x30 != get_streamfile_size(sf) &&
        align_size_to_block(stream_size + 0x30, 0x80) != get_streamfile_size(sf))
        goto fail;

    /*  HACK 1  */
    stream_size -= 0x20;
    /* technically the stream size is correct, however the final ADPCM frame
     * has the end flag 0x7 stored in the coef/shift byte for whatever reason
     * and the 2nd to last frame in most files has what seems like garbage(?)
     * so there's an audible click at the end from those.
     */

    /*  HACK 2  */
    if (is_id32be(0x10, sf, "tpad"))
        sample_rate = 44100; /* from the GC port */
    /* sample rate is valid for all files except Boostpad.vag, where this field
     * is uninitialized and instead has the string "tpad" (likely from the name)
     */

    channels = 1;
    loop_flag = 0;
    interleave = 0;
    start_offset = 0x30;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VAG_custom;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_none;
    vgmstream->sample_rate = sample_rate;
    vgmstream->interleave_block_size = interleave;
    vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);

    read_string(vgmstream->stream_name, 0x10 + 1, 0x20, sf); /* always "Evolution Games"? */

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
