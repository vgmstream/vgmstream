#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"

typedef enum { MFX, MFX_BANK, SFX_BANK, SBNK, FORM } musx_form;
typedef enum { PSX, DSP, XBOX, IMA, DAT, NGCA, PCM } musx_codec;
typedef struct {
    int big_endian;
    int version;
    size_t file_size;

    int total_subsongs;
    int is_old;

    off_t tables_offset;
    off_t loops_offset;
    off_t stream_offset;
    size_t stream_size;
    off_t coefs_offset;

    musx_form form;
    musx_codec codec;
    uint32_t platform;

    int channels;
    int sample_rate;
    int loop_flag;
    int32_t loop_start;
    int32_t loop_end;
    int32_t num_samples;
    int32_t loop_start_sample;
    int32_t loop_end_sample;
} musx_header;

static int parse_musx(STREAMFILE* sf, musx_header* musx);

/* MUSX - from Eurocom's games */
VGMSTREAM* init_vgmstream_musx(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    musx_header musx = {0};


    /* checks */
    /* .sfx: actual extension (extracted from bigfiles with sometimes encrypted names)
     * .musx: header id */
    if (!check_extensions(sf, "sfx,musx"))
        goto fail;
    if (!is_id32be(0x00,sf, "MUSX"))
        goto fail;

    if (!parse_musx(sf, &musx))
        goto fail;


    start_offset = musx.stream_offset;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(musx.channels, musx.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MUSX;
    vgmstream->sample_rate = musx.sample_rate;
    vgmstream->num_streams = musx.total_subsongs;
    vgmstream->stream_size = musx.stream_size;

    switch (musx.codec) {
        case PSX:
            vgmstream->num_samples = ps_bytes_to_samples(musx.stream_size, musx.channels);
            vgmstream->loop_start_sample = ps_bytes_to_samples(musx.loop_start, musx.channels);
            vgmstream->loop_end_sample = ps_bytes_to_samples(musx.loop_end, musx.channels);

            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x80;
            break;

        case DAT:
            vgmstream->num_samples = dat4_ima_bytes_to_samples(musx.stream_size, musx.channels);
            vgmstream->loop_start_sample = dat4_ima_bytes_to_samples(musx.loop_start, musx.channels);
            vgmstream->loop_end_sample = dat4_ima_bytes_to_samples(musx.loop_end, musx.channels);

            vgmstream->coding_type = coding_DAT4_IMA;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x20;
            break;

        case IMA:
            vgmstream->num_samples = ima_bytes_to_samples(musx.stream_size, musx.channels);
            vgmstream->loop_start_sample = musx.loop_start / 4; /* weird but needed */
            vgmstream->loop_end_sample = musx.loop_end / 4;

            vgmstream->coding_type = coding_DVI_IMA_int;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x01;
            break;

        case XBOX:
            vgmstream->num_samples = xbox_ima_bytes_to_samples(musx.stream_size, musx.channels);
            vgmstream->loop_start_sample = xbox_ima_bytes_to_samples(musx.loop_start, musx.channels);
            vgmstream->loop_end_sample = xbox_ima_bytes_to_samples(musx.loop_end, musx.channels);

            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            break;

        case DSP:
            vgmstream->num_samples = dsp_bytes_to_samples(musx.stream_size, musx.channels);
            vgmstream->loop_start_sample = dsp_bytes_to_samples(musx.loop_start, musx.channels);
            vgmstream->loop_end_sample = dsp_bytes_to_samples(musx.loop_end, musx.channels);

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x08;

            dsp_read_coefs(vgmstream,sf,musx.coefs_offset+0x1c, 0x60, musx.big_endian);
            dsp_read_hist(vgmstream,sf,musx.coefs_offset+0x40, 0x60, musx.big_endian);
            break;

        case PCM:
            //vgmstream->num_samples = pcm_bytes_to_samples(musx.stream_size, musx.channels, 16);
            //vgmstream->loop_start_sample = pcm_bytes_to_samples(musx.loop_start, musx.channels, 16);
            //vgmstream->loop_end_sample = pcm_bytes_to_samples(musx.loop_end, musx.channels, 16);

            vgmstream->coding_type = musx.big_endian ? coding_PCM16BE : coding_PCM16LE; /* only seen on PC but just in case */
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;

        case NGCA:
            if (!is_id32be(start_offset,sf, "NGCA"))
                goto fail;
            /* 0x04: size (stereo full-interleaves 2 NGCA headers but sizes count all) */
            /* 0x08: channels */
            /* 0x0c: coefs */
            musx.coefs_offset = start_offset;
            start_offset += 0x40;
            //musx.stream_size -= 0x40 * musx.channels; /* needed for interleave */

            //vgmstream->num_samples = dsp_bytes_to_samples(musx.stream_size - 0x40 * musx.channels, musx.channels);
            //vgmstream->loop_start_sample = dsp_bytes_to_samples(musx.loop_start + 0x40, musx.channels);
            //vgmstream->loop_end_sample = dsp_bytes_to_samples(musx.loop_end - 0x40 * musx.channels, musx.channels);

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = musx.stream_size / musx.channels;

            dsp_read_coefs(vgmstream, sf, musx.coefs_offset+0x0c, vgmstream->interleave_block_size, musx.big_endian);
            //dsp_read_hist(vgmstream, sf, musx.coefs_offset+0x30, 0x00, musx.big_endian); /* used? */

            break;

        default:
            goto fail;
    }

    if (musx.num_samples)
        vgmstream->num_samples = musx.num_samples;
    if (musx.loop_flag && musx.loop_start_sample)
        vgmstream->loop_start_sample = musx.loop_start_sample;
    if (musx.loop_flag && musx.loop_end_sample)
        vgmstream->loop_end_sample = musx.loop_end_sample;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static int parse_musx_stream(STREAMFILE* sf, musx_header* musx) {
    uint32_t (*read_u32)(off_t,STREAMFILE*) = NULL;
    int default_channels, default_sample_rate;

    if (musx->big_endian) {
        read_u32 = read_u32be;
    } else {
        read_u32 = read_u32le;
    }


    /* autodetect for older versions that have no info */
    if (musx->platform == 0) {
        if (musx->big_endian) {
            musx->platform = get_id32be("GC02"); /* (fake) */
        }
        else {
            int channels = musx->channels;
            off_t offset = musx->stream_offset;
            size_t max = 0x5000;
            if (max > musx->stream_size)
                max = musx->stream_size;
            if (!channels)
                channels = 2;

            /* since engine seems to hardcode codecs no apparent way to detect in some cases
             * [Sphinx and the Cursed Mummy (multi), Buffy the Vampire Slayer: Chaos Bleeds (multi)] */
            if (ps_check_format(sf, offset, max)) {
                musx->platform = get_id32be("PS2_");
            } else if (xbox_check_format(sf, offset, max, channels)) {
                musx->platform = get_id32be("XB02"); /* (fake) */
            }
            else {
                musx->platform = get_id32be("PC02"); /* (fake) */
            }
        }
    }


    /* defaults */
    switch(musx->platform) {

        case 0x5053325F: /* "PS2_" */
            default_channels = 2;
            default_sample_rate = 32000;
            musx->codec = PSX;
            break;

        case 0x47435F5F: /* "GC__" */
            default_channels = 2;
            default_sample_rate = 32000;
            musx->codec = DAT;
            break;

        case 0x47433032: /* "GC02" */
            default_channels = 2;
            default_sample_rate = 32000;
            if (musx->coefs_offset)
                musx->codec = DSP;
            else
                musx->codec = IMA;
            break;

        case 0x58425F5F: /* "XB__" */
        case 0x5842315F: /* "XB1_" */
            default_channels = 2;
            default_sample_rate = 44100;
            musx->codec = DAT;
            break;

        case 0x58423032: /* "XB02" */
            default_channels = 2;
            default_sample_rate = 44100;
            musx->codec = XBOX;
            break;

        case 0x5053505F: /* "PSP_" */
            default_channels = 2;
            default_sample_rate = 32768;
            musx->codec = PSX;
            break;

        case 0x5749495F: /* "WII_" */
            default_channels = 2;
            default_sample_rate = 32000;
            musx->codec = DAT;
            break;

        case 0x5053335F: /* "PS3_" */
            default_channels = 2;
            default_sample_rate = 44100;
            musx->codec = DAT;
            break;

        case 0x58455F5F: /* "XE__" */
            default_channels = 2;
            default_sample_rate = 32000;
            musx->codec = DAT;
            break;

        case 0x50435F5F: /* "PC__" */
            default_channels = 2;
            default_sample_rate = 44100;
            musx->codec = DAT;
            break;

        case 0x50433032: /* "PC02" */
            default_channels = 2;
            default_sample_rate = 32000;
            musx->codec = IMA;
            break;

        default:
            VGM_LOG("MUSX: unknown platform %x\n", musx->platform);
            goto fail;
    }

    if (musx->channels == 0)
        musx->channels = default_channels;
    if (musx->sample_rate == 0)
        musx->sample_rate = default_sample_rate;


    /* parse loops and other info */
    if (musx->tables_offset && musx->loops_offset) {
        int i, cues2_count;
        off_t cues2_offset;

        /* cue/stream position table thing */
        /* 0x00: cues1 entries (entry size 0x34 or 0x18)
         * 0x04: cues2 entries (entry size 0x20 or 0x14)
         * 0x08: header size (always 0x14)
         * 0x0c: cues2 start
         * 0x10: volume? (usually <= 100) */

        /* find loops (cues1 also seems to have this info but this looks ok) */
        cues2_count = read_u32(musx->loops_offset+0x04, sf);
        cues2_offset = musx->loops_offset + read_u32(musx->loops_offset+0x0c, sf);
        for (i = 0; i < cues2_count; i++) {
            uint32_t type, offset1, offset2;

            if (musx->is_old) {
                offset1 = read_u32(cues2_offset + i*0x20 + 0x04, sf);
                type    = read_u32(cues2_offset + i*0x20 + 0x08, sf);
                offset2 = read_u32(cues2_offset + i*0x20 + 0x14, sf);
            } else {
                offset1 = read_u32(cues2_offset + i*0x14 + 0x04, sf);
                type    = read_u32(cues2_offset + i*0x14 + 0x08, sf);
                offset2 = read_u32(cues2_offset + i*0x14 + 0x0c, sf);
            }

            /* other types (0x0a, 0x09) look like section/end markers, 0x06/07 only seems to exist once */
            if (type == 0x06 || type == 0x07) { /* loop / goto */
                musx->loop_start = offset2;
                musx->loop_end = offset1;
                musx->loop_flag = 1;
                break;
            }
        }
    }
    else if (musx->loops_offset && read_u32be(musx->loops_offset, sf) != 0xABABABAB) {
        /* parse loop table (loop starts are -1 if non-looping)
         * 0x00: version?
         * 0x04: flags? (&1=loops)
         * 0x08: loop start offset?
         * 0x0c: loop end offset?
         * 0x10: loop end sample
         * 0x14: loop start sample
         * 0x18: loop end offset
         * 0x1c: loop start offset */
        musx->loop_end_sample   = read_s32le(musx->loops_offset+0x10, sf);
        musx->loop_start_sample = read_s32le(musx->loops_offset+0x14, sf);
        musx->loop_end          = read_s32le(musx->loops_offset+0x18, sf);
        musx->loop_start        = read_s32le(musx->loops_offset+0x1c, sf);
        musx->num_samples       = musx->loop_end_sample; /* preferable even if not looping as some files have padding */
        musx->loop_flag         = (musx->loop_start_sample >= 0);
    }

    /* fix some v10 platform (like PSP) sizes */
    if (musx->stream_size == 0) {
        musx->stream_size = musx->file_size - musx->stream_offset;

        /* always padded to nearest 0x800 sector */
        if (musx->stream_size > 0x800) {
            uint8_t buf[0x800];
            int pos;
            off_t offset = musx->stream_offset + musx->stream_size - 0x800;

            if (read_streamfile(buf, offset, sizeof(buf), sf) != 0x800)
                goto fail;

            pos = 0x800 - 0x04;
            while (pos > 0) {
                if (get_u32be(buf + pos) != 0xABABABAB)
                    break;
                musx->stream_size -= 0x04;
                pos -= 0x04;
            }
        }
    }

    return 1;
fail:
    return 0;
}

//TODO: check possible info here:
// https://sphinxandthecursedmummy.fandom.com/wiki/SFX
static int parse_musx(STREAMFILE* sf, musx_header* musx) {
    int32_t (*read_s32)(off_t,STREAMFILE*) = NULL;
    uint32_t (*read_u32)(off_t,STREAMFILE*) = NULL;
    uint16_t (*read_u16)(off_t,STREAMFILE*) = NULL;
    int target_subsong = sf->stream_index;


    /* base header is LE */
    /* 0x04: file ID (referenced in external .h, sometimes files are named [prefix](id)[suffix].sfx too) */
    musx->version = read_u32le(0x08,sf);
    musx->file_size = read_u32le(0x0c,sf);

    switch(musx->version) {
        case 201:   /* Sphinx and the Cursed Mummy (PS2), Buffy the Vampire Slayer: Chaos Bleeds (PS2/GC/Xbox) */
        case 1:     /* Athens 2004 (PS2) */
            musx->platform = 0; /* guess later */
            musx->tables_offset = 0x10;
            musx->big_endian = guess_endian32(0x10, sf);
            musx->is_old = 1;
            break;

        case 4:     /* Spyro: A Hero's Tail (PS2/Xbox/GC), Athens 2004 (PC) */
        case 5:     /* Predator: Concrete Jungle (PS2/Xbox), Robots (GC) */
        case 6:     /* Batman Begins (GC), Ice Age 2 (PS2/PC) */
            musx->platform = read_u32be(0x10,sf);
            /* 0x14: some id/hash? */
            /* 0x18: platform number? */
            /* 0x1c: null */
            musx->tables_offset = 0x20;
            musx->big_endian = (musx->platform == get_id32be("GC__"));
            break;

        case 10:    /* 007: Quantum of Solace (PS2), Pirates of the Caribbean: At World's End (PSP), GoldenEye 007 (Wii), Rio (PS3) */
            musx->platform = read_u32be(0x10,sf);
            /* 0x14: null */
            /* 0x18: platform number? */
            /* 0x1c: null */
            /* 0x20: hash */
            musx->tables_offset = 0; /* no tables */
            musx->big_endian = (
                musx->platform == get_id32be("GC__") ||
                musx->platform == get_id32be("XE__") ||
                musx->platform == get_id32be("PS3_") ||
                musx->platform == get_id32be("WII_")
            );
            break;

        default:
            VGM_LOG("MUSX: unknown version\n");
            goto fail;
    }

    if (musx->big_endian) {
        read_s32 = read_s32be;
        read_u32 = read_u32be;
        read_u16 = read_u16be;
    } else {
        read_s32 = read_s32le;
        read_u32 = read_u32le;
        read_u16 = read_u16le;
    }


    /* MUSX has multiple forms which seem external/implicit info so we need some crude autodetection */
    if (musx->tables_offset) {
        off_t table1_offset, table2_offset /*, table3_offset, table4_offset*/;
        size_t /*table1_size, table2_size, table3_size,*/ table4_size;

        table1_offset   = read_u32(musx->tables_offset+0x00, sf);
      //table1_size     = read_u32(musx->tables_offset+0x04, sf);
        table2_offset   = read_u32(musx->tables_offset+0x08, sf);
      //table2_size     = read_u32(musx->tables_offset+0x0c, sf);
      //table3_offset   = read_u32(musx->tables_offset+0x10, sf);
      //table3_size     = read_u32(musx->tables_offset+0x14, sf);
      //table4_offset   = read_u32(musx->tables_offset+0x18, sf);
        table4_size     = read_u32(musx->tables_offset+0x1c, sf);

        if (table2_offset == 0 || table2_offset == 0xABABABAB) {
            /* possibly a collection of IDs */
            VGM_LOG("MUSX: unsupported ID type\n");
            goto fail;
        }
        else if (table4_size != 0 && table4_size != 0xABABABAB) {
            /* sfx banks have table1=id table? table2=header table, table=DSP coefs table (optional), table4=data */
            musx->form = SFX_BANK;
        }
        else if (read_u32(table1_offset+0x00, sf) < 0x9 &&
                 read_u32(table1_offset+0x08, sf) == 0x14 &&
                 read_u32(table1_offset+0x10, sf) <= 100) {
            /* streams have a info table with a certain format */
            musx->form = MFX;
        }
        else if (read_u32(table1_offset+0x00, sf) == 0 &&
                 read_u32(table1_offset+0x04, sf) > read_u32(table1_offset+0x00, sf) &&
                 read_u32(table1_offset+0x08, sf) > read_u32(table1_offset+0x04, sf)) {
            /* a list of offsets starting from 0, each stream then has info table at offset */
            musx->form = MFX_BANK;
        }
        else {
            VGM_LOG("MUSX: unsupported unknown type\n");
            goto fail;
        }
    }
    else {
        if (is_id32be(0x800,sf, "SBNK")) {
            /* SFX_BANK-like sound bank found on v10 Wii/PC games [Dead Space Extraction (Wii), G-Force (PC)] */
            musx->form = SBNK;
        }
        else if (is_id32be(0x800,sf, "FORM")) {
            /* RIFF-like sound bank found on v10 PSP games */
            musx->form = FORM;
        }
        else if (is_id32be(0x800,sf, "ESPD")) {
            /* projectdetails.sfx */
            goto fail;
        }
        else {
            /* simplest music type*/
            musx->form = MFX;
        }
    }


    /* parse known forms */
    switch(musx->form) {
        case MFX:

            if (musx->tables_offset) {
                musx->loops_offset  = read_u32(musx->tables_offset+0x00, sf);

                musx->stream_offset = read_u32(musx->tables_offset+0x08, sf);
                musx->stream_size   = read_u32(musx->tables_offset+0x0c, sf);
            }
            else {
                if (read_u32be(0x30, sf) != 0xABABABAB) {
                    uint32_t miniheader = read_u32be(0x40, sf);
                    switch(miniheader) {
                        case 0x44415434: /* "DAT4" */
                        case 0x44415435: /* "DAT5" */
                        case 0x44415438: /* "DAT8" */
                            /* found on PS3/Wii (but not always?) */
                            musx->stream_size = read_u32le(0x44, sf);
                            musx->channels    = read_u32le(0x48, sf);
                            musx->sample_rate = read_u32le(0x4c, sf);
                            musx->loops_offset = 0x50;
                            break;
                        default:
                            if (read_u32be(0x30, sf) == 0x00 &&
                                read_u32be(0x34, sf) == 0x00) {
                                /* no subheader [Spider-Man 4 (Wii)] */
                                musx->loops_offset = 0x00;
                            } else {
                                /* loop info only [G-Force (PS3)] */
                                musx->loops_offset = 0x30;
                            }
                            break;
                    }
                }

                musx->stream_offset = 0x800;
                musx->stream_size   = 0; /* read later */
            }

            if (!parse_musx_stream(sf, musx))
                goto fail;
            break;

        case MFX_BANK: {
            off_t target_offset, base_offset, data_offset;

            musx->total_subsongs = read_u32(musx->tables_offset+0x04, sf) / 0x04; /* size */
            if (target_subsong == 0) target_subsong = 1;
            if (target_subsong > musx->total_subsongs || musx->total_subsongs <= 0) goto fail;

            base_offset = read_u32(musx->tables_offset+0x00, sf);
            data_offset = read_u32(musx->tables_offset+0x08, sf);

            target_offset = read_u32(base_offset + (target_subsong - 1)*0x04, sf) + data_offset;

            /* 0x00: id? */
            musx->stream_offset = read_u32(target_offset + 0x04, sf) + data_offset;
            musx->stream_size   = read_u32(target_offset + 0x08, sf);
            musx->loops_offset  = target_offset + 0x0c;

            /* this looks correct for PS2 and Xbox, not sure about GC */
            musx->channels = 1;
            musx->sample_rate = 22050;

            if (!parse_musx_stream(sf, musx))
                goto fail;
            break;
        }

        case SFX_BANK: {
            off_t target_offset, head_offset, coef_offset, data_offset;
            size_t coef_size;

          //unk_offset  = read_u32(musx->tables_offset+0x00, sf); /* ids and cue-like config? */
            head_offset = read_u32(musx->tables_offset+0x08, sf);
            coef_offset = read_u32(musx->tables_offset+0x10, sf);
            coef_size   = read_u32(musx->tables_offset+0x14, sf);
            data_offset = read_u32(musx->tables_offset+0x18, sf);

            musx->total_subsongs = read_u32(head_offset+0x00, sf);
            if (target_subsong == 0) target_subsong = 1;
            if (target_subsong > musx->total_subsongs || musx->total_subsongs <= 0) goto fail;


            if (musx->is_old) {
                target_offset = head_offset + 0x04 + (target_subsong - 1)*0x28;

                /* 0x00: flag? */
                musx->stream_offset = read_u32(target_offset + 0x04, sf) + data_offset;
                musx->stream_size   = read_u32(target_offset + 0x08, sf);
                musx->sample_rate   = read_u32(target_offset + 0x0c, sf);
                /* 0x10: size? */
                /* 0x14: channels? */
                /* 0x18: always 4? */
                musx->coefs_offset  = read_u32(target_offset + 0x1c, sf) + coef_offset; /* may be set even for non-GC */
                /* 0x20: null? */
                /* 0x24: sub-id? */
                musx->channels = 1;
            }
            else {
                target_offset = head_offset + 0x04 + (target_subsong - 1)*0x20;

                /* 0x00: flag? */
                musx->stream_offset = read_u32(target_offset + 0x04, sf) + data_offset;
                musx->stream_size   = read_u32(target_offset + 0x08, sf);
                musx->sample_rate   = read_u32(target_offset + 0x0c, sf);
                /* 0x10: size? */
                musx->coefs_offset  = read_u32(target_offset + 0x14, sf) + coef_offset; /* may be set even for non-GC */
                /* 0x18: null? */
                /* 0x1c: sub-id? */
                musx->channels = 1;
            }

            if (coef_size == 0)
                musx->coefs_offset = 0; /* disable for later detection */

            if (!parse_musx_stream(sf, musx))
                goto fail;
            break;
        }

        case SBNK: {
            off_t target_offset, head_offset, data_offset;
            uint8_t codec = 0;
            uint32_t version;

            if (!is_id32be(0x800 + 0x00, sf, "SBNK"))
                goto fail;

            version = read_u32(0x800 + 0x04, sf);
            if (version == 0x2A) {
                /* - Goldeneye 007 (X360) */
                /* 0x08: "COM " */
                /* 0x0c: file ID (same as base file) */
                /* 0x10: some ID? */
                musx->tables_offset = 0x814;
            }
            else {
                /* - v0x12 (all others) */
                /* 0x08: file ID (same as base file) */
                /* 0x0c: some ID? */
                musx->tables_offset = 0x810;
            }
                
            /* 0x00: table1 count */
            /* 0x04: table1 offset (from here) */
            /* 0x08: table2 count */
            /* 0x0c: table2 offset (from here) */

            /* 0x10: table3 count */
            /* 0x14: table3 offset (from here) */
            /* 0x18: table4 count */
            /* 0x1c: table4 offset (from here) */

            /* 0x20: table5 count (waves) */
            /* 0x24: table5 offset (from here) */
            /* 0x28: table6 count */
            /* 0x2c: table6 offset (from here) */

            /* 0x30: table7 count */
            /* 0x34: table7 offset (from here) */
            /* 0x38: data size */
            /* 0x3c: data offset (absolute in older versions) */

            musx->total_subsongs = read_u32(musx->tables_offset+0x20, sf);
            if (target_subsong == 0) target_subsong = 1;
            if (target_subsong > musx->total_subsongs || musx->total_subsongs <= 0) goto fail;

            if (version == 0x2A) {
                ;VGM_LOG("MUSX: unknown version format\n");
                goto fail;
                /* subheader is 0x10 but variable?, offset table may be table 7? 
                  - 0x00: ID?
                  - 0x04: samples?
                  - 0x08: sample rate
                  - 0x0a: codec?
                  - 0x0b: channels
                  - 0x0c: size?
                */
            }
            else {
                head_offset = read_u32(musx->tables_offset+0x24, sf) + musx->tables_offset + 0x24;
                data_offset = read_u32(musx->tables_offset+0x3c, sf);

                target_offset = head_offset + (target_subsong - 1) * 0x1c;
                //;VGM_LOG("MUSX: ho=%lx, do=%lx, to=%lx\n", head_offset, data_offset, target_offset);

                /* 0x00: subfile ID */
                musx->num_samples       = read_s32(target_offset + 0x04, sf);
                musx->loop_start_sample = read_s32(target_offset + 0x08, sf); /* -1 if no loop */
                musx->sample_rate       = read_u16(target_offset + 0x0c, sf);
                codec                   = read_u8 (target_offset + 0x0e, sf);
                musx->channels          = read_u8 (target_offset + 0x0f, sf);
                musx->stream_offset     = read_u32(target_offset + 0x10, sf) + data_offset;
                musx->stream_size       = read_u32(target_offset + 0x14, sf);
                musx->loop_start        = read_s32(target_offset + 0x18, sf);
            }

            musx->loop_end_sample   = musx->num_samples;
            musx->loop_flag         = (musx->loop_start_sample >= 0);

            switch(codec) {
                case 0x11: musx->codec = DAT; break;
                case 0x13: musx->codec = NGCA; break;
                case 0x14: musx->codec = PCM; break;
                default: 
                    VGM_LOG("MUSX: unknown SBNK codec %x\n", codec);
                    goto fail;
            }

            break;
        }

        default:
            VGM_LOG("MUSX: unsupported form\n");
            goto fail;
    }


    return 1;
fail:
    return 0;
}
