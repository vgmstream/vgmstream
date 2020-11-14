#include "meta.h"
#include "../coding/coding.h"

#include "nus3bank_streamfile.h"

typedef enum { IDSP, IVAG, BNSF, RIFF, RIFF_XMA2, OPUS, RIFF_ENC, } nus3bank_codec;

/* .nus3bank - Namco's newest audio container [Super Smash Bros (Wii U), THE iDOLM@STER 2 (PS3/X360)] */
VGMSTREAM* init_vgmstream_nus3bank(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t tone_offset = 0, pack_offset = 0, name_offset = 0, subfile_offset = 0;
    size_t name_size = 0, subfile_size = 0;
    nus3bank_codec codec;
    const char* fake_ext;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    /* .nub2: early [THE iDOLM@STER 2 (PS3/X360)]
     * .nus3bank: standard */
    if (!check_extensions(sf, "nub2,nus3bank"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x4E555333) /* "NUS3" */
        goto fail;
    if (read_u32be(0x08,sf) != 0x42414E4B) /* "BANK" */
        goto fail;
    if (read_u32be(0x0c,sf) != 0x544F4320) /* "TOC\0" */
        goto fail;

    /* header is always LE, while contained files may use another endianness */

    /* parse TOC with all existing chunks and sizes (offsets must be derived) */
    {
        int i;
        off_t offset = 0x14 + read_u32le(0x10, sf); /* TOC size */
        size_t chunk_count = read_u32le(0x14, sf); /* rarely not 7 (ex. SMB U's snd_bgm_CRS12_Simple_Result_Final) */

        for (i = 0; i < chunk_count; i++) {
            uint32_t chunk_id  = read_u32be(0x18+(i*0x08)+0x00, sf);
            size_t chunk_size  = read_u32le(0x18+(i*0x08)+0x04, sf);

            switch(chunk_id) {
                case 0x544F4E45: /* "TONE": stream info */
                    tone_offset = 0x08 + offset;
                    break;
                case 0x5041434B: /* "PACK": audio streams */
                    pack_offset = 0x08 + offset;
                    break;

                case 0x50524F50: /* "PROP": project info */
                case 0x42494E46: /* "BINF": bank info (filename) */
                case 0x47525020: /* "GRP ": cues/events with names? */
                case 0x44544F4E: /* "DTON": related to GRP? */
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
        size_t entries = read_u32le(tone_offset+0x00, sf);

        /* get actual number of subsongs */
        total_subsongs = 0;
        if (target_subsong == 0) target_subsong = 1;

        for (i = 0; i < entries; i++) {
            off_t offset, tone_header_offset, stream_name_offset, stream_offset;
            size_t tone_header_size, stream_name_size, stream_size;
            uint8_t flags2;

            tone_header_offset = read_u32le(tone_offset+0x04+(i*0x08)+0x00, sf);
            tone_header_size  = read_u32le(tone_offset+0x04+(i*0x08)+0x04, sf);

            offset = tone_offset + tone_header_offset;
            //;VGM_LOG("NUS3BANK: tone at %lx, size %x\n", tone_offset + tone_header_offset, tone_header_size);

            if (tone_header_size <= 0x0c) {
                //VGM_LOG("NUS3BANK: bad tone at %lx, size %x\n", tone_offset + tone_header_offset, tone_header_size);
                continue; /* ignore non-sounds */
            }

            /* 0x00: type? normally 0x00 and rarely 0x09  */
            /* 0x04: usually -1, found when tone is not a stream (most flags are off too) */
            /* 0x06: flags1 */
            flags2 = read_8bit(offset + 0x07, sf);
            offset += 0x08;

            /* flags3-6 (early .nub2 and some odd non-stream don't have them) */
            if (flags2 & 0x80) {
                offset += 0x04;
            }

            stream_name_size = read_8bit(offset + 0x00, sf); /* includes null */
            stream_name_offset = offset + 0x01;
            offset += align_size_to_block(0x01 + stream_name_size, 0x04); /* padded if needed */

            /* 0x00: subtype? should be 0 */
            if (read_u32le(offset + 0x04, sf) != 0x08) { /* flag? */
                //;VGM_LOG("NUS3BANK: bad tone type at %lx, size %x\n", tone_offset + tone_header_offset, tone_header_size);
                continue;
            }

            stream_offset = read_u32le(offset + 0x08, sf) + pack_offset;
            stream_size   = read_u32le(offset + 0x0c, sf);
            //;VGM_LOG("NUS3BANK: so=%lx, ss=%x\n", stream_offset, stream_size);

            /* Beyond are a bunch of sub-chunks of unknown size with floats and stuff, that seemingly
             * appear depending on flags1-6. One is a small stream header, which contains basic
             * sample rate/channels/loops/etc, but it's actually optional and maybe controlled by
             * flags 3-6 (ex. not found in .nub2) */

            /* happens in some sfx packs (ex. Taiko no Tatsujin Switch's se_minigame) */
            if (stream_size == 0) {
                //;VGM_LOG("NUS3BANK: bad tone stream size at %lx, size %x\n", tone_offset + tone_header_offset, tone_header_size);
                continue;
            }


            total_subsongs++;
            if (total_subsongs == target_subsong) {
                //;VGM_LOG("NUS3BANK: subsong header offset %lx\n", offset);
                subfile_offset = stream_offset;
                subfile_size = stream_size;
                name_size = stream_name_size;
                name_offset = stream_name_offset;
            }
            /* continue counting subsongs */
        }

        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
        if (subfile_offset == 0) {
            VGM_LOG("NUS3BANK: subsong not found\n");
            goto fail;
        }

        //todo improve, codec may be in one of the tone sub-chunks (or other chunk? one bank seems to use one codec)
        codec_id = read_u32be(subfile_offset + 0x00, sf);
        switch(codec_id) {
            case 0x49445350: /* "IDSP" [Super Smash Bros. for 3DS (3DS)] */
                codec = IDSP;
                fake_ext = "idsp";
                break;

            case 0x52494646: { /* "RIFF" [THE iDOLM@STER 2 (PS3), Mario Kart Arcade GP DX (PC), idolm@ster: Platinum Stars (PS4)] */
                uint16_t format = read_u16le(subfile_offset + 0x14, sf);
                if (format == 0x0166) { /* Tekken Tag Tournament 2 (X360) */ 
                    codec = RIFF_XMA2;
                    fake_ext = "xma";
                }
                else {
                    codec = RIFF;
                    fake_ext = "wav"; //TODO: works but should have better detection
                }
                break;
            }

            case 0x4F505553: /* "OPUS" [Taiko no Tatsujin (Switch)] */
                codec = OPUS;
                fake_ext = "opus";
                break;

            case 0x424E5346: /* "BNSF" [Naruto Shippuden Ultimate Ninja Storm 4 (PC)] */
                codec = BNSF;
                fake_ext = "bnsf";
                break;

            case 0x49564147: /* "IVAG" [THE iDOLM@STER 2 (PS3), THE iDOLM@STER: Gravure For You! (PS3)] */
                codec = IVAG;
                fake_ext = "ivag";
                break;

            case 0x552AAF17: /* "RIFF" with encrypted header (not data) [THE iDOLM@STER 2 (X360)] */
                codec = RIFF_ENC;
                fake_ext = "xma";
                break;

            default:
                VGM_LOG("NUS3BANK: unknown codec %x\n", codec_id);
                goto fail;
        }
    }

    //;VGM_LOG("NUS3BANK: subfile=%lx, size=%x, %s\n", subfile_offset, subfile_size, fake_ext);

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, fake_ext);
    if (!temp_sf) goto fail;

    /* init the VGMSTREAM */
    switch(codec) {
        case IDSP:
            vgmstream = init_vgmstream_idsp_namco(temp_sf);
            if (!vgmstream) goto fail;
            break;

        case OPUS:
            vgmstream = init_vgmstream_opus_nus3(temp_sf);
            if (!vgmstream) goto fail;
            break;

        case RIFF:
            vgmstream = init_vgmstream_riff(temp_sf);
            if (!vgmstream) goto fail;
            break;

        case BNSF:
            vgmstream = init_vgmstream_bnsf(temp_sf);
            if (!vgmstream) goto fail;
            break;

        case IVAG:
            vgmstream = init_vgmstream_ivag(temp_sf);
            if (!vgmstream) goto fail;
            break;

        case RIFF_XMA2:
            vgmstream = init_vgmstream_xma(temp_sf);
            if (!vgmstream) goto fail;
            break;

        case RIFF_ENC:
            vgmstream = init_vgmstream_nus3bank_encrypted(temp_sf);
            if (!vgmstream) goto fail;
            break;

        default:
            goto fail;
    }

    vgmstream->num_streams = total_subsongs;
    if (name_offset)
        read_string(vgmstream->stream_name, name_size, name_offset, sf);


    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

/* encrypted RIFF from the above, in case kids try to extract and play single files */
VGMSTREAM* init_vgmstream_nus3bank_encrypted(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;


    /* checks */
    if (!check_extensions(sf, "nus3bank,xma"))
        goto fail;
    if (read_u32be(0x00, sf) != 0x552AAF17) /* "RIFF" encrypted */
        goto fail;

    temp_sf = setup_nus3bank_streamfile(sf, 0x00);
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_xma(temp_sf);
    if (!vgmstream) goto fail;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
