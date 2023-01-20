#include "meta.h"
#include "../coding/coding.h"


/* .ISB - Creative ISACT (Interactive Spatial Audio Composition Tools) middleware [Psychonauts (PC), Mass Effect (multi)] */
VGMSTREAM* init_vgmstream_isb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset = 0, name_offset = 0, nfld_offset = 0;
    size_t stream_size = 0, name_size = 0, nfld_size = 0;
    int loop_flag = 0, channels = 0, sample_rate = 0, codec = 0, pcm_bytes = 0, bps = 0;
    int total_subsongs, target_subsong = sf->stream_index;
    uint32_t (*read_u32me)(off_t,STREAMFILE*);
    uint32_t (*read_u32ce)(off_t,STREAMFILE*);
    int big_endian;


    /* checks */
    if (!check_extensions(sf, "isb"))
        goto fail;

    big_endian = read_u32be(0x00,sf) == get_id32be("FFIR"); /* PS3, most X360 */
    read_u32me = big_endian ? read_u32be : read_u32le; /* machine endianness... */
    read_u32ce = big_endian ? read_u32le : read_u32be; /* chunks change with endianness but this just reads as BE */

    if (read_u32ce(0x00,sf) != get_id32be("RIFF"))
        goto fail;
    if (read_u32me(0x04,sf) + 0x08 != get_streamfile_size(sf))
        goto fail;
    if (read_u32ce(0x08,sf) != get_id32be("isbf"))
        goto fail;

    /* some files have a companion .icb, seems to be a cue file pointing here */

    /* format is RIFF with many custom chunks, apparently for their DAW-like editor with
     * complex functions, but most seem always included by default and unused, and games
     * seems to use the format as a simple audio bank. Psychonauts Xbox/PS2 doesn't use ISACT. */

    {
        off_t offset, max_offset, header_offset = 0, suboffset, submax_offset;
        size_t header_size = 0;

        total_subsongs = 0; /* not specified */
        if (target_subsong == 0) target_subsong = 1;

        /* parse base RIFF */
        offset = 0x0c;
        max_offset = get_streamfile_size(sf);
        while (offset < max_offset) {
            uint32_t chunk_type = read_u32ce(offset + 0x00,sf);
            uint32_t chunk_size = read_u32me(offset + 0x04,sf);
            offset += 0x08;

            switch(chunk_type) {
                case 0x4C495354: /* "LIST" */
                    if (read_u32ce(offset, sf) == get_id32be("samp")) {
                        /* sample header */
                        total_subsongs++;
                        if (target_subsong == total_subsongs && header_offset == 0) {
                            header_offset = offset;
                            header_size = chunk_size;
                        }
                    }
                    else if (read_u32ce(offset, sf) == get_id32be("fldr")) {
                        /* subfolder with another LIST inside, for example "stingers" > N smpl (seen in some music_bank) */
                        off_t current_nfld_offset = 0;
                        size_t current_nfld_size = 0;

                        suboffset = offset + 0x04;
                        submax_offset = offset + chunk_size;
                        while (suboffset < submax_offset) {
                            uint32_t subchunk_type = read_u32ce(suboffset + 0x00,sf);
                            uint32_t subchunk_size = read_u32me(suboffset + 0x04,sf);
                            suboffset += 0x08;

                            if (subchunk_type == get_id32be("titl")) {
                                /* should go first in fldr*/
                                current_nfld_offset = suboffset;
                                current_nfld_size = subchunk_size;
                            }
                            else if (subchunk_type == get_id32be("LIST")) {
                                uint32_t subsubchunk_type = read_u32ce(suboffset, sf);

                                if (subsubchunk_type == get_id32be("samp")) {
                                    total_subsongs++;
                                    if (target_subsong == total_subsongs && header_offset == 0) {
                                        header_offset = suboffset;
                                        header_size = chunk_size;
                                        nfld_offset = current_nfld_offset;
                                        nfld_size = current_nfld_size;
                                    }
                                }
                                else if (subsubchunk_type == get_id32be("fldr")) {
                                    VGM_LOG("ISB: subfolder with subfolder at %lx\n", suboffset);
                                    goto fail;
                                }

                                //break; /* there can be N subLIST+samps */
                            }

                            suboffset += subchunk_size;
                        }
                    }

                    break;

                default: /* most are common chunks at the start that seem to contain defaults */
                    break;
            }

            offset += chunk_size; /* no chunk 1-byte padding */
        }

        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
        if (header_offset == 0) goto fail;

        /* parse header inside LIST */
        offset = header_offset + 0x04;
        max_offset = offset + header_size;
        while (offset < max_offset) {
            uint32_t chunk_type = read_u32ce(offset + 0x00,sf);
            uint32_t chunk_size = read_u32me(offset + 0x04,sf);
            offset += 0x08;

            switch(chunk_type) {
                case 0x7469746C: /* "titl" */
                    name_offset = offset;
                    name_size = chunk_size;
                    break;

                case 0x63686E6B: /* "chnk" */
                    channels = read_u32me(offset + 0x00, sf);
                    break;

                case 0x73696E66: /* "sinf" */
                    /* 0x00: null? */
                    /* 0x04: some value? */
                    sample_rate = read_u32me(offset + 0x08, sf);
                    pcm_bytes = read_u32me(offset + 0x0c, sf);
                    bps = read_u16le(offset + 0x10, sf);
                    /* 0x12: some value? */
                    break;

                case 0x636D7069: /* "cmpi" */
                    codec = read_u32me(offset + 0x00, sf);
                    if (read_u32me(offset + 0x04, sf) != codec) {
                        VGM_LOG("ISB: unknown compression repeat\n");
                        goto fail;
                    }
                    /* 0x08: extra value for some codecs? */
                    /* 0x0c: block size when codec is XBOX-IMA */
                    /* 0x10: null? */
                    /* 0x14: flags? */
                    break;

                case 0x64617461: /* "data" */
                    start_offset = offset;
                    stream_size = chunk_size;
                    break;

                default: /* most of the same default chunks */
                    break;
            }

            offset += chunk_size;
        }

        if (start_offset == 0)
            goto fail;
    }


    /* some files are marked with "loop" but have value 0? */
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_ISB;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    if (name_offset) {
        /* UTF16 but only uses lower bytes */
        char name[256];
        char nfld[256];
        
        /* should read string or set '\0' is no size is set/incorrect */
        if (name_size >= sizeof(name))
            name_size = sizeof(name) - 1;
        read_string_utf16(name, name_size, name_offset, sf, big_endian);

        if (nfld_size >= sizeof(nfld))
            nfld_size = sizeof(nfld) - 1;
        read_string_utf16(nfld, nfld_size, nfld_offset, sf, big_endian);

        if (nfld[0] && name[0]) {
            snprintf(vgmstream->stream_name,STREAM_NAME_SIZE, "%s/%s", nfld, name);
        }
        else if (name[0]) {
            snprintf(vgmstream->stream_name,STREAM_NAME_SIZE, "%s", name);
        }
        /* there is also a "titl" for the bank, but it's just the filename so probably unwanted */
    }

    switch(codec) {
        case 0x00:
            if (bps == 8) {
                vgmstream->coding_type = coding_PCM8_U;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = 0x01;
            }
            else {
                vgmstream->coding_type = coding_PCM16LE;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = 0x02;
            }
            vgmstream->num_samples = pcm_bytes_to_samples(stream_size, channels, bps); /* unsure about pcm_bytes */
            break;

        case 0x01:
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = xbox_ima_bytes_to_samples(stream_size, channels); /* pcm_bytes has excess data */
            break;

#ifdef VGM_USE_VORBIS
        case 0x02:
            vgmstream->codec_data = init_ogg_vorbis(sf, start_offset, stream_size, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_OGG_VORBIS;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = pcm_bytes / channels / (bps/8);
            break;
#endif

#ifdef VGM_USE_FFMPEG
        case 0x04: {
            off_t fmt_offset = start_offset;
            size_t fmt_size = 0x20;

            start_offset += fmt_size;
            stream_size -= fmt_size;

            /* XMA1 "fmt" chunk (BE, unlike the usual LE) */
            vgmstream->codec_data = init_ffmpeg_xma_chunk(sf, start_offset, stream_size, fmt_offset, fmt_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = pcm_bytes / channels / (bps/8);
            xma_fix_raw_samples(vgmstream, sf, start_offset, stream_size, fmt_offset, 1,1);
            break;
        }
#endif

        case 0x05: {
            //TODO: improve
            VGMSTREAM *temp_vgmstream = NULL;
            STREAMFILE *temp_sf = NULL;

            temp_sf = setup_subfile_streamfile(sf, start_offset, stream_size, "msf");
            if (!temp_sf) goto fail;

            temp_vgmstream = init_vgmstream_msf(temp_sf);
            if (temp_vgmstream) {
                temp_vgmstream->num_streams = vgmstream->num_streams;
                temp_vgmstream->stream_size = vgmstream->stream_size;
                temp_vgmstream->meta_type = vgmstream->meta_type;
                strcpy(temp_vgmstream->stream_name, vgmstream->stream_name);

                close_streamfile(temp_sf);
                close_vgmstream(vgmstream);
                return temp_vgmstream;
            }
            else {
                close_streamfile(temp_sf);
                goto fail;
            }

            break;

        }

        default: /* according to press releases ISACT may support WMA */
            VGM_LOG("ISB: unknown codec %i\n", codec);
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
