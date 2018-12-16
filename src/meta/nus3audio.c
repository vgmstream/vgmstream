#include "meta.h"
#include "../coding/coding.h"

typedef enum { IDSP, OPUS, } nus3audio_codec;

/* .nus3audio - Namco's newest newest audio container [Super Smash Bros. Ultimate (Switch)] */
VGMSTREAM * init_vgmstream_nus3audio(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t subfile_offset = 0, name_offset = 0;
    size_t subfile_size = 0;
    nus3audio_codec codec;
    const char* fake_ext = NULL;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    if (!check_extensions(streamFile, "nus3audio"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4E555333) /* "NUS3" */
        goto fail;
    if (read_32bitLE(0x04,streamFile) + 0x08 != get_streamfile_size(streamFile))
        goto fail;
    if (read_32bitBE(0x08,streamFile) != 0x41554449) /* "AUDI" */
        goto fail;


    /* parse existing chunks */
    {
        off_t offset = 0x0c;
        size_t file_size = get_streamfile_size(streamFile);
        uint32_t codec_id = 0;

        total_subsongs = 0;

        while (offset < file_size) {
            uint32_t chunk_id  = (uint32_t)read_32bitBE(offset+0x00, streamFile);
            size_t chunk_size  =   (size_t)read_32bitLE(offset+0x04, streamFile);

            switch(chunk_id) {
                case 0x494E4458: /* "INDX": audio index */
                    total_subsongs = read_32bitLE(offset+0x08 + 0x00,streamFile);
                    if (target_subsong == 0) target_subsong = 1;
                    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
                    break;

                case 0x4E4D4F46: /* "NMOF": name offsets (absolute, inside TNNM) */
                    name_offset = read_32bitLE(offset+0x08 + 0x04*(target_subsong-1),streamFile);
                    break;

                case 0x41444F46: /* "ADOF": audio offsets (absolute, inside PACK) */
                    subfile_offset = read_32bitLE(offset+0x08 + 0x08*(target_subsong-1) + 0x00,streamFile);
                    subfile_size   = read_32bitLE(offset+0x08 + 0x08*(target_subsong-1) + 0x04,streamFile);
                    break;

                case 0x544E4944: /* "TNID": tone ids? */
                case 0x544E4E4D: /* "TNNM": tone names */
                case 0x4A554E4B: /* "JUNK": padding */
                case 0x5041434B: /* "PACK": main data */
                default:
                    break;
            }

            offset += 0x08 + chunk_size;
        }

        if (total_subsongs == 0 || subfile_offset == 0 || subfile_size == 0) {
            VGM_LOG("NUS3AUDIO: subfile not found\n");
            goto fail;
        }

        codec_id = read_32bitBE(subfile_offset, streamFile);
        switch(codec_id) {
            case 0x49445350: /* "IDSP" */
                codec = IDSP;
                fake_ext = "idsp";
                break;
            case 0x4F505553: /* "OPUS" */
                codec = OPUS;
                fake_ext = "opus";
                break;
            default:
                VGM_LOG("NUS3AUDIO: unknown codec %x\n", codec_id);
                goto fail;
        }
    }


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
        default:
            goto fail;
    }

    vgmstream->num_streams = total_subsongs;
    if (name_offset) /* null-terminated */
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,streamFile);

    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}


