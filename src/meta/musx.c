#include "meta.h"
#include "../coding/coding.h"

typedef enum { MFX, MFX_BANK, SFX_BANK, SBNK, FORM } musx_form;
typedef enum { PSX, DSP, XBOX, IMA, DAT } musx_codec;
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

static int parse_musx(STREAMFILE *streamFile, musx_header *musx);

/* MUSX - from Eurocom's games */
VGMSTREAM * init_vgmstream_musx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    musx_header musx = {0};


    /* checks */
    /* .sfx: actual extension (extracted from bigfiles with sometimes encrypted names)
     * .musx: header id */
    if (!check_extensions(streamFile, "sfx,musx"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4D555358) /* "MUSX" */
        goto fail;

    if (!parse_musx(streamFile, &musx))
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

            dsp_read_coefs(vgmstream,streamFile,musx.coefs_offset+0x1c, 0x60, musx.big_endian);
            dsp_read_hist(vgmstream,streamFile,musx.coefs_offset+0x40, 0x60, musx.big_endian);
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

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static int parse_musx_stream(STREAMFILE *streamFile, musx_header *musx) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int default_channels, default_sample_rate;

    if (musx->big_endian) {
        read_32bit = read_32bitBE;
    } else {
        read_32bit = read_32bitLE;
    }


    /* autodetect for older versions that have no info */
    if (musx->platform == 0) {
        if (musx->big_endian) {
            musx->platform = 0x47433032; /* "GC02" (fake) */
        }
        else {
            off_t offset = musx->stream_offset;
            size_t max = 0x5000;
            if (max > musx->stream_size)
                max = musx->stream_size;

            if (ps_check_format(streamFile, offset, max)) {
                musx->platform = 0x5053325F; /* "PS2_" */
            } else {
                musx->platform = 0x58423032; /* "XB02" (fake) */
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
            default_sample_rate = 44100;
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
        cues2_count = read_32bit(musx->loops_offset+0x04, streamFile);
        cues2_offset = musx->loops_offset + read_32bit(musx->loops_offset+0x0c, streamFile);
        for (i = 0; i < cues2_count; i++) {
            uint32_t type, offset1, offset2;

            if (musx->is_old) {
                offset1 = read_32bit(cues2_offset + i*0x20 + 0x04, streamFile);
                type    = read_32bit(cues2_offset + i*0x20 + 0x08, streamFile);
                offset2 = read_32bit(cues2_offset + i*0x20 + 0x14, streamFile);
            } else {
                offset1 = read_32bit(cues2_offset + i*0x14 + 0x04, streamFile);
                type    = read_32bit(cues2_offset + i*0x14 + 0x08, streamFile);
                offset2 = read_32bit(cues2_offset + i*0x14 + 0x0c, streamFile);
            }

            /* other types (0x0a, 0x09) look like section/end markers, 0x06/07 only seems to exist once */
            if (type == 0x06 || type == 0x07) {
                musx->loop_start = offset2;
                musx->loop_end = offset1;
                musx->loop_flag = 1;
                break;
            }
        }
    }
    else if (musx->loops_offset && read_32bitBE(musx->loops_offset, streamFile) != 0xABABABAB) {
        /* parse loop table (loop starts are -1 if non-looping)
         * 0x00: version?
         * 0x04: flags? (&1=loops)
         * 0x08: loop start offset?
         * 0x0c: loop end offset?
         * 0x10: loop end sample
         * 0x14: loop start sample
         * 0x18: loop end offset
         * 0x1c: loop start offset */
        musx->loop_end_sample   = read_32bitLE(musx->loops_offset+0x10, streamFile);
        musx->loop_start_sample = read_32bitLE(musx->loops_offset+0x14, streamFile);
        musx->loop_end          = read_32bitLE(musx->loops_offset+0x18, streamFile);
        musx->loop_start        = read_32bitLE(musx->loops_offset+0x1c, streamFile);
        musx->num_samples       = musx->loop_end_sample; /* preferable even if not looping as some files have padding */
        musx->loop_flag         = (musx->loop_start_sample >= 0);
    }

    /* fix some v10 sizes */
    if (musx->stream_size == 0) {
        off_t offset;
        musx->stream_size = musx->file_size - musx->stream_offset;

        /* remove padding */
        offset = musx->stream_offset + musx->stream_size - 0x04;
        while (offset > 0) {
            if (read_32bit(offset, streamFile) != 0xABABABAB)
                break;
            musx->stream_size -= 0x04;
            offset -= 0x04;
        }
    }

    return 1;
fail:
    return 0;
}

static int parse_musx(STREAMFILE *streamFile, musx_header *musx) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int target_subsong = streamFile->stream_index;


    /* base header is LE */
    /* 0x04: file ID (referenced in external .h, sometimes files are named HC(id).sfx too) */
    musx->version = read_32bitLE(0x08,streamFile);
    musx->file_size = read_32bitLE(0x0c,streamFile);


    switch(musx->version) {
        case 201:   /* Sphinx and the Cursed Mummy (PS2), Buffy the Vampire Slayer: Chaos Bleeds (PS2/GC/Xbox) */
        case 1:     /* Athens 2004 (PS2) */
            musx->platform = 0; /* guess later */
            musx->tables_offset = 0x10;
            musx->big_endian = guess_endianness32bit(0x10, streamFile);
            musx->is_old = 1;
            break;

        case 4:     /* Spyro: A Hero's Tail (PS2/Xbox/GC) */
        case 5:     /* Predator: Concrete Jungle (PS2/Xbox), Robots (GC) */
        case 6:     /* Batman Begins (GC), Ice Age 2 (PS2) */
            musx->platform = read_32bitBE(0x10,streamFile);
            /* 0x14: some id/hash? */
            /* 0x18: platform number? */
            /* 0x1c: null */
            musx->tables_offset = 0x20;
            musx->big_endian = (musx->platform == 0x47435F5F); /* "GC__" */
            break;

        case 10:    /* 007: Quantum of Solace (PS2), Pirates of the Caribbean: At World's End (PSP), GoldenEye 007 (Wii), Rio (PS3) */
            musx->platform = read_32bitBE(0x10,streamFile);
            /* 0x14: null */
            /* 0x18: platform number? */
            /* 0x1c: null */
            /* 0x20: hash */
            musx->tables_offset = 0; /* no tables */
            musx->big_endian = (musx->platform == 0x5749495F || musx->platform == 0x5053335F); /* "GC__" / "PS3_" (only after header) */
            break;

        default:
            goto fail;
    }

    if (musx->big_endian) {
        read_32bit = read_32bitBE;
    } else {
        read_32bit = read_32bitLE;
    }


    /* MUSX has multiple forms which seem external/implicit info so we need some crude autodetection */
    if (musx->tables_offset) {
        off_t table1_offset, table2_offset /*, table3_offset, table4_offset*/;
        size_t /*table1_size, table2_size, table3_size,*/ table4_size;

        table1_offset   = read_32bit(musx->tables_offset+0x00, streamFile);
      //table1_size     = read_32bit(musx->tables_offset+0x04, streamFile);
        table2_offset   = read_32bit(musx->tables_offset+0x08, streamFile);
      //table2_size     = read_32bit(musx->tables_offset+0x0c, streamFile);
      //table3_offset   = read_32bit(musx->tables_offset+0x10, streamFile);
      //table3_size     = read_32bit(musx->tables_offset+0x14, streamFile);
      //table4_offset   = read_32bit(musx->tables_offset+0x18, streamFile);
        table4_size     = read_32bit(musx->tables_offset+0x1c, streamFile);

        if (table2_offset == 0 || table2_offset == 0xABABABAB) {
            /* possibly a collection of IDs */
            goto fail;
        }
        else if (table4_size != 0 && table4_size != 0xABABABAB) {
            /* sfx banks have table1=id table? table2=header table, table=DSP coefs table (optional), table4=data */
            musx->form = SFX_BANK;
        }
        else if (read_32bit(table1_offset+0x00, streamFile) < 0x9 &&
                 read_32bit(table1_offset+0x08, streamFile) == 0x14 &&
                 read_32bit(table1_offset+0x10, streamFile) <= 100) {
            /* streams have a info table with a certain format */
            musx->form = MFX;
        }
        else if (read_32bit(table1_offset+0x00, streamFile) == 0 &&
                 read_32bit(table1_offset+0x04, streamFile) > read_32bit(table1_offset+0x00, streamFile) &&
                 read_32bit(table1_offset+0x08, streamFile) > read_32bit(table1_offset+0x04, streamFile)) {
            /* a list of offsets starting from 0, each stream then has info table at offset */
            musx->form = MFX_BANK;
        }
        else {
            goto fail;
        }
    }
    else {
        if (read_32bitBE(0x800,streamFile) == 0x53424E4B) { /* "SBNK" */
            /* SFX_BANK-like sound bank found on v10 Wii games */
            musx->form = SBNK;
        }
        else if (read_32bitBE(0x800,streamFile) == 0x464F524D) { /* "FORM" */
            /* RIFF-like sound bank found on v10 PSP games */
            musx->form = FORM;
        }
        else if (read_32bitBE(0x800,streamFile) == 0x45535044) { /* "ESPD" */
            /* projectdetails.sfx */
            goto fail;
        }
        else {
            musx->form = MFX;
        }
    }


    /* parse known forms */
    switch(musx->form) {
        case MFX:

            if (musx->tables_offset) {
                musx->loops_offset  = read_32bit(musx->tables_offset+0x00, streamFile);

                musx->stream_offset = read_32bit(musx->tables_offset+0x08, streamFile);
                musx->stream_size   = read_32bit(musx->tables_offset+0x0c, streamFile);
            }
            else {
                if (read_32bitBE(0x30, streamFile) != 0xABABABAB) {
                    uint32_t miniheader = read_32bitBE(0x40, streamFile);
                    switch(miniheader) {
                        case 0x44415434: /* "DAT4" */
                        case 0x44415438: /* "DAT8" */
                            /* found on PS3/Wii (but not always?) */
                            musx->stream_size = read_32bitLE(0x44, streamFile);
                            musx->channels    = read_32bitLE(0x48, streamFile);
                            musx->sample_rate = read_32bitLE(0x4c, streamFile);
                            musx->loops_offset = 0x50;
                            break;
                        default:
                            musx->loops_offset = 0x30;
                            break;
                    }
                }

                musx->stream_offset = 0x800;
                musx->stream_size   = 0; /* read later */
            }

            if (!parse_musx_stream(streamFile, musx))
                goto fail;
            break;

        case MFX_BANK: {
            off_t target_offset, base_offset, data_offset;

            musx->total_subsongs = read_32bit(musx->tables_offset+0x04, streamFile) / 0x04; /* size */
            if (target_subsong == 0) target_subsong = 1;
            if (target_subsong > musx->total_subsongs || musx->total_subsongs <= 0) goto fail;

            base_offset = read_32bit(musx->tables_offset+0x00, streamFile);
            data_offset = read_32bit(musx->tables_offset+0x08, streamFile);

            target_offset = read_32bit(base_offset + (target_subsong - 1)*0x04, streamFile) + data_offset;

            /* 0x00: id? */
            musx->stream_offset = read_32bit(target_offset + 0x04, streamFile) + data_offset;
            musx->stream_size   = read_32bit(target_offset + 0x08, streamFile);
            musx->loops_offset  = target_offset + 0x0c;

            /* this looks correct for PS2 and Xbox, not sure about GC */
            musx->channels = 1;
            musx->sample_rate = 22050;

            if (!parse_musx_stream(streamFile, musx))
                goto fail;
            break;
        }

        case SFX_BANK: {
            off_t target_offset, head_offset, coef_offset, data_offset;
            size_t coef_size;

          //unk_offset  = read_32bit(musx->tables_offset+0x00, streamFile); /* ids and cue-like config? */
            head_offset = read_32bit(musx->tables_offset+0x08, streamFile);
            coef_offset = read_32bit(musx->tables_offset+0x10, streamFile);
            coef_size   = read_32bit(musx->tables_offset+0x14, streamFile);
            data_offset = read_32bit(musx->tables_offset+0x18, streamFile);

            musx->total_subsongs = read_32bit(head_offset+0x00, streamFile);
            if (target_subsong == 0) target_subsong = 1;
            if (target_subsong > musx->total_subsongs || musx->total_subsongs <= 0) goto fail;


            if (musx->is_old) {
                target_offset = head_offset + 0x04 + (target_subsong - 1)*0x28;

                /* 0x00: flag? */
                musx->stream_offset = read_32bit(target_offset + 0x04, streamFile) + data_offset;
                musx->stream_size   = read_32bit(target_offset + 0x08, streamFile);
                musx->sample_rate   = read_32bit(target_offset + 0x0c, streamFile);
                /* 0x10: size? */
                /* 0x14: channels? */
                /* 0x18: always 4? */
                musx->coefs_offset  = read_32bit(target_offset + 0x1c, streamFile) + coef_offset; /* may be set even for non-GC */
                /* 0x20: null? */
                /* 0x24: sub-id? */
                musx->channels = 1;
            }
            else {
                target_offset = head_offset + 0x04 + (target_subsong - 1)*0x20;

                /* 0x00: flag? */
                musx->stream_offset = read_32bit(target_offset + 0x04, streamFile) + data_offset;
                musx->stream_size   = read_32bit(target_offset + 0x08, streamFile);
                musx->sample_rate   = read_32bit(target_offset + 0x0c, streamFile);
                /* 0x10: size? */
                musx->coefs_offset  = read_32bit(target_offset + 0x14, streamFile) + coef_offset; /* may be set even for non-GC */
                /* 0x18: null? */
                /* 0x1c: sub-id? */
                musx->channels = 1;
            }


            VGM_LOG("to=%lx, %lx, %x\n", target_offset, musx->stream_offset, musx->stream_size);

            if (coef_size == 0)
                musx->coefs_offset = 0; /* disable for later detection */

            if (!parse_musx_stream(streamFile, musx))
                goto fail;
            break;
        }

        default:
            VGM_LOG("MUSX: unknown form\n");
            goto fail;
    }


    return 1;
fail:
    return 0;
}
