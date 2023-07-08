#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/endianness.h"


/* RAWI - from Torus games "SqueakStream" samples */
VGMSTREAM* init_vgmstream_rawi(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sb = NULL;
    STREAMFILE* sn = NULL;
    uint32_t start_offset, name_offset, extn_offset, interleave;
    int channels, loop_flag, codec, sample_rate;
    int32_t num_samples, loop_start, loop_end;


    /* checks */
    bool big_endian = false;
    if (is_id32be(0x00,sf, "RAWI"))
        big_endian = false;
    else if (is_id32be(0x00,sf, "IWAR"))
        big_endian = true; /* Wii/PS3 */
    else
        return NULL;
    //TODO: handle first version used in Scooby Doo! First Frights (similar but larger fields and no header ID)

    /* (extensionless): no known extension */
    if (!check_extensions(sf,""))
        return NULL;

    read_s32_t read_s32 = big_endian ? read_s32be : read_s32le;
    read_u32_t read_u32 = big_endian ? read_u32be : read_u32le;

    /* mini header with a string to the external asset; on Wii this string is also in a separate file */

    if (read_u8(0x04,sf) != 0x01) /* version? */
        return NULL;
    codec = read_u8(0x05,sf);
    channels = read_u8(0x06,sf);
    /* 0x07: null */
    num_samples = read_s32(0x08, sf);
    sample_rate = read_s32(0x0c, sf);
    loop_start  = read_s32(0x10, sf);
    loop_end    = read_s32(0x14, sf);
  //etbl_offset = read_u32(0x18, sf);
    name_offset = read_u32(0x1c, sf);
    /* 0x20: null, unknown values (sometimes floats) */
    interleave  = read_u32(0x38, sf);
    /* extra values, then DSP coefs if needed, then asset name (header size is not exact) */

    extn_offset  = (name_offset >> 24) & 0xFF; /*  if name is external, sub-offset inside that file */
    name_offset = (name_offset >>  0) & 0xFFFFFF; /* if name is external, default/unused (same with etbl_offset) */

    /* simplify as Wii defines both and uses a separate file, PS3 only defines extn and doesn't use separate */
    if (extn_offset && !name_offset) {
        name_offset = extn_offset;
        extn_offset = 0;
    }

    loop_flag = loop_end > 0;

    start_offset = 0x00;

    /* open external asset */
    {
        char asset_name[0x20]; /* "(8-byte crc).raw", "MU(6-byte crc).raw" */

        
        if (extn_offset) {
            sn = open_streamfile_by_ext(sf, "asset"); /* unknown real extension, based on debug strings */
            if (!sn) {
                vgm_logi("RAWI: external name '.asset' not found (put together)\n");
                goto fail;
            }

            read_string(asset_name, sizeof(asset_name), extn_offset, sn);
        }
        else {
            read_string(asset_name, sizeof(asset_name), name_offset, sf);
        }

        /* try to open external asset in various ways, since this format is a bit hard to use */

        /* "(asset name)": plain as found  */
        if (!sb){
            sb = open_streamfile_by_filename(sf, asset_name);
        }

        /* "sound/(asset name)": most common way to store files */
        char path_name[256];
        snprintf(path_name, sizeof(path_name), "sound/%s", asset_name);
        if (!sb){
            sb = open_streamfile_by_filename(sf, path_name);
        }

        /* "(header name).raw": for renamed files */
        if (!sb){
            sb = open_streamfile_by_ext(sf, "raw");
        }

        if (!sb) {
            vgm_logi("RAWI: external file '%s' not found (put together)\n", asset_name);
            goto fail;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RAWI;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end + 1;

    switch(codec) {
        case 0x00: /* Turbo Super Stunt Squad (Wii/3DS), Penguins of Madagascar (Wii/U/3DS) */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;
            //vgmstream->interleave_last_block_size = ...; /* apparently padded */

            /* etbl_offset defines N coef offset per channel (with external name, etbl_offset is ignored and offsets start at 0x00 in .asset instead) 
             * but in practice this seem fixed */
            dsp_read_coefs(vgmstream, sf, 0x40, 0x30, big_endian);
            dsp_read_hist(vgmstream, sf, 0x40 + 0x24, 0x30, big_endian);
            break;

        case 0x01: /* Falling Skies The Game (PC) */
            vgmstream->coding_type = coding_PCM16BE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave; /* not 0x02 */

        case 0x02: /* Falling Skies The Game (X360) */
            vgmstream->coding_type = coding_PCM16BE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave; /* not 0x02 */

            /* etbl_offset may set offsets to RIFF fmts per channel) */
            break;

        case 0x03: /* How to Train Your Dragon 2 (PS3), Falling Skies The Game (PS3) */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;
            break;

        case 0x05: /*  Scooby Doo and the Spooky Swamp (DS) */
            vgmstream->coding_type = coding_PCM8;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;
            break;

        case 0x09: /* Turbo Super Stunt Squad (DS) */
            vgmstream->coding_type = coding_MS_IMA;
            vgmstream->layout_type = layout_none;
            //vgmstream->interleave_block_size = interleave; /* unused? (mono) */
            vgmstream->frame_size = 0x20;
            break;

        default:
            vgm_logi("RAWI: unknown codec %x (report)\n", codec);
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sb, start_offset))
        goto fail;
    close_streamfile(sb);
    close_streamfile(sn);
    return vgmstream;
fail:
    close_streamfile(sb);
    close_streamfile(sn);
    close_vgmstream(vgmstream);
    return NULL;
}
