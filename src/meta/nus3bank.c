#include "meta.h"
#include "../coding/coding.h"

typedef enum { /*XMA_RAW, ATRAC3,*/ IDSP, ATRAC9, OPUS, BNSF, /*PCM, XMA_RIFF*/ } nus3bank_codec;

/* .nus3bank - Namco's newest audio container [Super Smash Bros (Wii U), idolmaster (PS4))] */
VGMSTREAM * init_vgmstream_nus3bank(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t tone_offset = 0, pack_offset = 0, name_offset = 0, subfile_offset = 0;
    size_t name_size = 0, subfile_size = 0;
    nus3bank_codec codec;
    const char* fake_ext;
    int total_subsongs, target_subsong = streamFile->stream_index;

    /* checks */
    if (!check_extensions(streamFile, "nus3bank"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4E555333) /* "NUS3" */
        goto fail;
    if (read_32bitBE(0x08,streamFile) != 0x42414E4B) /* "BANK" */
        goto fail;
    if (read_32bitBE(0x0c,streamFile) != 0x544F4320) /* "TOC\0" */
        goto fail;

    /* parse TOC with all existing chunks and sizes (offsets must be derived) */
    {
        int i;
        off_t offset = 0x14 + read_32bitLE(0x10, streamFile); /* TOC size */
        size_t chunk_count = read_32bitLE(0x14, streamFile); /* rarely not 7 (ex. SMB U's snd_bgm_CRS12_Simple_Result_Final) */

        for (i = 0; i < chunk_count; i++) {
            uint32_t chunk_id  = (uint32_t)read_32bitBE(0x18+(i*0x08)+0x00, streamFile);
            size_t chunk_size  =   (size_t)read_32bitLE(0x18+(i*0x08)+0x04, streamFile);

            switch(chunk_id) {
                case 0x544F4E45: /* "TONE": stream info */
                    tone_offset = 0x08 + offset;
                    break;
                case 0x5041434B: /* "PACK": audio streams */
                    pack_offset = 0x08 + offset;
                    break;

                case 0x50524F50: /* "PROP": project info */
                case 0x42494E46: /* "BINF": bank info (filename) */
                case 0x47525020: /* "GRP ": ? */
                case 0x44544F4E: /* "DTON": ? */
                case 0x4D41524B: /* "MARK": ? */
                case 0x4A554E4B: /* "JUNK": padding */
                default:
                    break;
            }

            offset += 0x08 + chunk_size;
        }

        if (tone_offset == 0 || pack_offset == 0) {
            VGM_LOG("NUS3BANK: chunks found\n");
            goto fail;
        }
    }


    /* parse tones */
    {
        int i;
        uint32_t codec_id = 0;
        size_t entries = read_32bitLE(tone_offset+0x00, streamFile);

        /* get actual number of subsongs */
        total_subsongs = 0;
        if (target_subsong == 0) target_subsong = 1;

        for (i = 0; i < entries; i++) {
            off_t header_offset, header_suboffset, stream_name_offset, stream_offset;
            size_t stream_name_size, stream_size;
            off_t tone_header_offset = read_32bitLE(tone_offset+0x04+(i*0x08)+0x00, streamFile);
            size_t tone_header_size  = read_32bitLE(tone_offset+0x04+(i*0x08)+0x04, streamFile);

            if (tone_header_size <= 0x0c) {
                continue; /* ignore non-sounds */
            }

            header_offset = tone_offset + tone_header_offset;

            stream_name_size = read_8bit(header_offset+0x0c, streamFile); /* includes null */
            stream_name_offset = header_offset+0x0d;
            header_suboffset = 0x0c + 0x01 + stream_name_size;
            if (header_suboffset % 0x04) /* padded */
                header_suboffset += (0x04 - (header_suboffset % 0x04));
            if (read_32bitLE(header_offset+header_suboffset+0x04, streamFile) != 0x08) {
                continue; /* ignore non-sounds, too */
            }

            stream_offset = read_32bitLE(header_offset+header_suboffset+0x08, streamFile) + pack_offset;
            stream_size   = read_32bitLE(header_offset+header_suboffset+0x0c, streamFile);
            if (stream_size == 0) {
                continue; /* happens in some sfx packs */
            }

            total_subsongs++;
            if (total_subsongs == target_subsong) {
                //;VGM_LOG("NUS3BANK: subsong header offset %lx\n", header_offset);
                subfile_offset = stream_offset;
                subfile_size = stream_size;
                name_size = stream_name_size;
                name_offset = stream_name_offset;
                //todo improve, codec may be related to header values at 0x00/0x06/0x08
                codec_id = read_32bitBE(subfile_offset, streamFile);
            }
            /* continue counting subsongs */
        }

        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
        if (subfile_offset == 0 || codec_id == 0) {
            VGM_LOG("NUS3BANK: subsong not found\n");
            goto fail;
        }

        switch(codec_id) {
            case 0x49445350: /* "IDSP" [Super Smash Bros. for 3DS (3DS)] */
                codec = IDSP;
                fake_ext = "idsp";
                break;
            case 0x52494646: /* "RIFF" [idolm@ster: Platinum Stars (PS4)] */
                //todo this can be standard PCM RIFF too, ex. Mario Kart Arcade GP DX (PC) (works but should have better detection)
                codec = ATRAC9;
                fake_ext = "at9";
                break;
            case 0x4F505553: /* "OPUS" [Taiko no Tatsujin (Switch)] */
                codec = OPUS;
                fake_ext = "opus";
                break;
            case 0x424E5346: /* "BNSF" [Naruto Shippuden Ultimate Ninja Storm 4 (PC)] */
                codec = BNSF;
                fake_ext = "bnsf";
                break;
            default:
                VGM_LOG("NUS3BANK: unknown codec %x\n", codec_id);
                goto fail;
        }
    }

    //;VGM_LOG("NUS3BANK: subfile=%lx, size=%x\n", subfile_offset, subfile_size);

    temp_streamFile = setup_subfile_streamfile(streamFile, subfile_offset,subfile_size, fake_ext);
    if (!temp_streamFile) goto fail;

    /* init the VGMSTREAM */
    switch(codec) {
        case IDSP:
            vgmstream = init_vgmstream_idsp_nus3(temp_streamFile);
            if (!vgmstream) goto fail;
            break;
        case OPUS:
            vgmstream = init_vgmstream_opus_nus3(temp_streamFile);
            if (!vgmstream) goto fail;
            break;
        case ATRAC9:
            vgmstream = init_vgmstream_riff(temp_streamFile);
            if (!vgmstream) goto fail;
            break;
        case BNSF:
            vgmstream = init_vgmstream_bnsf(temp_streamFile);
            if (!vgmstream) goto fail;
            break;
        default:
            goto fail;
    }

    vgmstream->num_streams = total_subsongs;
    if (name_offset)
        read_string(vgmstream->stream_name,name_size, name_offset,streamFile);


    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
