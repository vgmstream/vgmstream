#include "meta.h"
#include "../coding/coding.h"
#include "zsnd_streamfile.h"


/* ZSND - Z-Axis/Vicarious Visions games [X-Men Legends II (multi), Marvel Ultimate Alliance (multi)] */
VGMSTREAM * init_vgmstream_zsnd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t start_offset, name_offset;
    size_t stream_size, name_size;
    int loop_flag, channel_count, sample_rate, layers, layers2 = 0;
    uint32_t codec;
    int total_subsongs, target_subsong = streamFile->stream_index;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    /* .zss/zsm: standard
     * .ens/enm: same for PS2
     * .zsd: normal or compact [BMX XXX (Xbox), Aggresive Inline (Xbox)] */
    if (!check_extensions(streamFile, "zss,zsm,ens,enm,zsd"))
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


    /* parse header tables */
    {
        off_t header2_offset, header3_offset;
        int   table2_entries, table3_entries;
        off_t table2_body, table3_body;
        int is_v1, i;


        /* multiple config tables:
         *  0x00: entries
         *  0x04: table head offset
         *  0x08: table body offset
         *
         * table heads are 0x08 * entries:
         *  0x00 = id? (crc-like, varies between tables but consistent between platforms)
         *  0x04 = entry? (number, also in table2_body?)
         *
         * main tables:
         *  table1: stream cues? (entry=0x18)
         *  table2: stream heads (optional, rarely not all stream bodies may have heads)
         *  table3: stream body
         *  table4: unknown, very rare, some kind of seek table with numbers going up? (Aggresive Inline: speech01.zsd)
         *  table5: unknown, very rare, (X-Men Legends II: boss4_m.zsm)
         *  table6/7: not seen
         *
         * table1 may have more entries than table2/3, and sometimes isn't set
         */

        /* V1 has no table heads, rare [Aggresive Inline (Xbox)]
         * no apparent flag but we can test if table heads offsets appear */
        is_v1 = read_32bit(0x14,streamFile) <= read_32bit(0x1c,streamFile) &&
                read_32bit(0x1c,streamFile) <= read_32bit(0x24,streamFile) &&
                read_32bit(0x24,streamFile) <= read_32bit(0x2c,streamFile) &&
                read_32bit(0x2c,streamFile) <= read_32bit(0x34,streamFile) &&
                read_32bit(0x34,streamFile) <= read_32bit(0x3c,streamFile) &&
                read_32bit(0x3c,streamFile) <= read_32bit(0x44,streamFile);

        if (!is_v1) {
            table2_entries = read_32bit(0x1c,streamFile);
            table2_body    = read_32bit(0x24,streamFile);

            table3_entries = read_32bit(0x28,streamFile);
            table3_body    = read_32bit(0x30,streamFile);
        }
        else {
            table2_entries = read_32bit(0x18,streamFile);
            table2_body    = read_32bit(0x1C,streamFile);

            table3_entries = read_32bit(0x20,streamFile);
            table3_body    = read_32bit(0x24,streamFile);
        }

        total_subsongs = table3_entries;

        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        switch (codec) {
            case 0x50432020: /* "PC  " */
                if (table2_entries == 0) goto fail;

                header2_offset = table2_body + 0x18*(target_subsong-1);
                layers       = read_16bit(header2_offset + 0x02,streamFile);
                sample_rate  = read_32bit(header2_offset + 0x04,streamFile);

                header3_offset = table3_body + 0x4c*(target_subsong-1);
                start_offset = read_32bit(header3_offset + 0x00,streamFile);
                stream_size  = read_32bit(header3_offset + 0x04,streamFile);
                name_offset  = header3_offset + 0x0c;
                name_size    = 0x40;
                break;

            case 0x58424F58: { /* "XBOX" */
                size_t entry2_size = is_v1 || check_extensions(streamFile, "zsd") ? 0x14 : 0x1c;

                /* BMX has unordered stream headers, and not every stream has a header */
                header2_offset = 0;
                for (i = 0; i < table2_entries; i++) {
                    int16_t id = read_16bit(table2_body + entry2_size*i + 0x00,streamFile);

                    if (id >= 0 && id + 1 != target_subsong) /* can be -1 == deleted entry */
                        continue;
                    header2_offset = table2_body + entry2_size*i;
                    break;
                }

                if (header2_offset == 0) {
                    if (table2_entries > 0) {
                        /* seems usable for sfx, meh */
                        header2_offset = table2_body + entry2_size*0;
                        layers       = read_16bit(header2_offset + 0x02,streamFile);
                        sample_rate  = read_32bit(header2_offset + 0x04,streamFile);
                    }
                    else {
                        layers       = 0;
                        sample_rate  = 0;
                    }
                }
                else {
                    layers       = read_16bit(header2_offset + 0x02,streamFile);
                    sample_rate  = read_32bit(header2_offset + 0x04,streamFile);
                    if (entry2_size > 0x18) {
                        layers2 = read_32bit(header2_offset + 0x18,streamFile);
                    }
                }

                header3_offset = table3_body + 0x54*(target_subsong-1);
                start_offset = read_32bit(header3_offset + 0x00,streamFile);
                stream_size  = read_32bit(header3_offset + 0x04,streamFile);
                /* 0x08: flags? related to looping? (not channels) */
              //loop_end     = read_32bit(header3_offset + 0x10,streamFile);
                name_offset  = header3_offset + 0x14;
                name_size    = 0x40;

                /* early games sometimes don't seem to have info or headers, not sure how to detect better
                 * ex. Aggresive Inline speech (1ch) vs music (2ch), or BMX cutscenes (2ch) */
                if (sample_rate == 0) {
                    int is_music = 0;
                    if (is_v1) {
                        char filename[PATH_LIMIT];

                        /* stream length isn't enough */
                        get_streamfile_filename(streamFile, filename, sizeof(filename));
                        is_music = strcmp(filename, "music.zsd") == 0;
                    }
                    else {
                        is_music = stream_size > 0x20000;
                    }

                    if (is_music) {
                        layers       = 0x02;
                        sample_rate  = 44100;
                    }
                    else {
                        layers       = 0x00;
                        sample_rate  = is_v1 ? 16000 : 22050; /* some BMX need 16000 but can't detect? */
                    }
                }

                break;
            }

            case 0x50533220: /* "PS2 " (also for PSP) */
                if (table2_entries == 0) goto fail;

                header2_offset = table2_body + 0x10*(target_subsong-1);
                sample_rate  = read_16bit(header2_offset + 0x02,streamFile);
                layers       = read_16bit(header2_offset + 0x04,streamFile);

                header3_offset = table3_body + 0x08*(target_subsong-1);
                start_offset = read_32bit(header3_offset + 0x00,streamFile);
                stream_size  = read_32bit(header3_offset + 0x04,streamFile);
                name_offset  = 0;
                name_size    = 0;

                /* pitch value, with 0x1000=44100 (voices vary quite a bit, ex. X-Men Legends 2) */
                sample_rate = round10(sample_rate * 44100.0 / 4096.0);
                /* there may be some rounding for lower values, ex 0x45A = 11993.99 ~= 12000, though not all: 
                 * 0x1000 = 44100, 0x0800 = 22050, 0x0687 ~= 18000, 0x05ce ~= 16000, 0x045a ~= 12000, 0x0400 = 11025 */
                break;

            case 0x47435542: /* "GCUB" (also for Wii) */
                header2_offset = table2_body + 0x18*(target_subsong-1);
                layers        = read_16bit(header2_offset + 0x02,streamFile);
                sample_rate   = read_32bit(header2_offset + 0x04,streamFile);

                header3_offset = table3_body + 0x0c*(target_subsong-1);
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
            case 0x01: channel_count = 1; break; /* set when looping? */
            case 0x02: channel_count = 2; break;
            case 0x22: channel_count = 4; break;
            default:
                VGM_LOG("ZSND: unknown layers\n");
                goto fail;
        }

        if (layers2) {
            channel_count = channel_count * layers2;
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

            /* very rarely entries refer to external .wma, but redoing the logic to handle only real
             * streams handle is a pain, so signal this case with an empty file [Aggresive Inline (Xbox)] */
            if (vgmstream->num_samples == 0) {
                vgmstream->num_samples = 48;
            }
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
