#include "meta.h"
#include "../coding/coding.h"
#include "zsnd_streamfile.h"


/* ZSND - Vicarious Visions games [X-Men Legends II (multi), Marvel Ultimate Alliance (multi)] */
VGMSTREAM * init_vgmstream_zsnd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t start_offset, name_offset;
    size_t stream_size, name_size;
    int loop_flag, channel_count, sample_rate, layers;
    uint32_t codec;
    int total_subsongs, target_subsong = streamFile->stream_index;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    /* .zss/zsm: standard, .ens/enm: same for PS2 */
    if (!check_extensions(streamFile, "zss,zsm,ens,enm"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x5A534E44) /* "ZSND" */
        goto fail;
    /* probably zss=stream, zsm=memory; no diffs other than size */

    codec = read_32bitBE(0x04,streamFile);
    /* 0x08: file size, but slightly bigger (+0x01~04) in some platforms */
    /* 0x0c: header end/first stream start (unneeded as all offsets are absolute) */

    if (codec == 0x47435542) { /* "GCUB" */
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    }
    else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }


    /* parse header tables (7*0x0c) */
    {
        off_t header2_offset, header3_offset;

        /* table2: stream head */
        int   table2_entries = read_32bit(0x1c,streamFile);
      //off_t table2_head    = read_32bit(0x20,streamFile);
        off_t table2_body    = read_32bit(0x24,streamFile);

        /* table3: stream body */
        int   table3_entries = read_32bit(0x28,streamFile);
      //off_t table3_head    = read_32bit(0x2c,streamFile);
        off_t table3_body    = read_32bit(0x30,streamFile);

        /* table1: stream cues? (entry=0x18)
         * tables 4-7 seem reserved with 0 entries and offsets to header end,
         * though table5 can be seen in boss4_m.zsm (1 entry) */

        /* table heads are always 0x08 * entries */
        /* 0x00 = ? (varies between tables but consistent between platforms) */
        /* 0x04 = id? (also in table2_body at 0x00?) */

        /* table1 may have more entries than table2/3 */
        if (table2_entries != table3_entries) {
            VGM_LOG("ZSND: table2/3 entries don't match\n");
            goto fail;
        }


        total_subsongs = table2_entries;
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        switch (codec) {
            case 0x50432020: /* "PC  " */
                header2_offset = table2_body + 0x18*(target_subsong-1);
                header3_offset = table3_body + 0x4c*(target_subsong-1);

                layers       = read_16bit(header2_offset + 0x02,streamFile);
                sample_rate  = read_32bit(header2_offset + 0x04,streamFile);
                start_offset = read_32bit(header3_offset + 0x00,streamFile);
                stream_size  = read_32bit(header3_offset + 0x04,streamFile);
                name_offset  = header3_offset + 0x0c;
                name_size    = 0x40;
                break;

            case 0x58424F58: /* "XBOX" */
                header2_offset = table2_body + 0x1c*(target_subsong-1);
                header3_offset = table3_body + 0x54*(target_subsong-1);

                layers       = read_16bit(header2_offset + 0x02,streamFile);
                sample_rate  = read_32bit(header2_offset + 0x04,streamFile);
                start_offset = read_32bit(header3_offset + 0x00,streamFile);
                stream_size  = read_32bit(header3_offset + 0x04,streamFile);
                name_offset  = header3_offset + 0x14;
                name_size    = 0x40;
                break;

            case 0x50533220: /* "PS2 " (also for PSP) */
                header2_offset = table2_body + 0x10*(target_subsong-1);
                header3_offset = table3_body + 0x08*(target_subsong-1);

                sample_rate  = read_16bit(header2_offset + 0x02,streamFile);
                layers       = read_16bit(header2_offset + 0x04,streamFile);
                start_offset = read_32bit(header3_offset + 0x00,streamFile);
                stream_size  = read_32bit(header3_offset + 0x04,streamFile);
                name_offset  = 0;
                name_size    = 0;
                switch(sample_rate) {
                    case 0x0800: sample_rate = 22050; break;
                    case 0x0687: sample_rate = 18000; break;
                    case 0x05ce: sample_rate = 16000; break;
                    case 0x0400: sample_rate = 11025; break;
                    default:
                        VGM_LOG("ZSND: unknown sample_rate %x at %x\n", sample_rate, (uint32_t)header2_offset);
                        goto fail;
                }
                break;

            case 0x47435542: /* "GCUB" (also for Wii) */
                header2_offset = table2_body + 0x18*(target_subsong-1);
                header3_offset = table3_body + 0x0c*(target_subsong-1);

                layers        = read_16bit(header2_offset + 0x02,streamFile);
                sample_rate   = read_32bit(header2_offset + 0x04,streamFile);
                start_offset  = read_32bit(header3_offset + 0x00,streamFile);
                stream_size   = read_32bit(header3_offset + 0x04,streamFile);
                /* 0x08: "DSP " for some reason */
                name_offset   = 0;
                name_size     = 0;
                break;

            default:
                goto fail;
        }

        /* maybe flags? */
        switch (layers) {
            case 0x00: channel_count = 1; break;
            case 0x01: channel_count = 1; break; /* related to looping? */
            case 0x02: channel_count = 2; break;
            case 0x22: channel_count = 4; break;
            default:
                VGM_LOG("ZSND: unknown layers\n");
                goto fail;
        }

        loop_flag = 0;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_ZSND;
    vgmstream->sample_rate = sample_rate;

    switch (codec) {
        case 0x50432020: /* "PC  " */
            vgmstream->coding_type = coding_IMA;
            vgmstream->layout_type = layout_none;
            //todo interleaved stereo (needs to adapt decoder)
            //vgmstream->layout_type = layout_interleave; /* interleaved stereo for >2ch*/
            //vgmstream->interleave_block_size = 0x2000 * 2 / channel_count;

            vgmstream->num_samples = ima_bytes_to_samples(stream_size, channel_count);
            break;

        case 0x58424F58: /* "XBOX" */
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_interleave; /* interleaved stereo for >2ch*/
            vgmstream->interleave_block_size = 0x9000 * 2 / channel_count;

            vgmstream->num_samples = xbox_ima_bytes_to_samples(stream_size, channel_count);
            break;

        case 0x50533220: /* "PS2 " (also for PSP) */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x800;

            vgmstream->num_samples = ps_bytes_to_samples(stream_size, channel_count);
            break;

        case 0x47435542: /* "GCUB" (also for Wii) */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x8000;

            /* has a full DSP header, but num_samples may vary slighly between channels, so calc manually */
            dsp_read_coefs_be(vgmstream, streamFile, start_offset+0x1c,0x60);
            dsp_read_hist_be(vgmstream, streamFile, start_offset+0x40, 0x60);
            start_offset += 0x60*channel_count;
            stream_size -= 0x60*channel_count;

            vgmstream->num_samples = dsp_bytes_to_samples(stream_size, channel_count);
            break;

        default:
            goto fail;
    }

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    if (name_offset) {
        read_string(vgmstream->stream_name,name_size, name_offset,streamFile);
    }


    temp_streamFile = setup_zsnd_streamfile(streamFile, start_offset, stream_size); /* fixes last interleave reads */
    if (!temp_streamFile) goto fail;

    if (!vgmstream_open_stream(vgmstream,temp_streamFile,start_offset))
        goto fail;
    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
