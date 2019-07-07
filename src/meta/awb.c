#include "meta.h"
#include "../coding/coding.h"

typedef enum { ADX, HCA, AT9, VAG } awb_type;

/* CRI AFS2, container of streaming ADX or HCA, often (but not always) together with a .acb CUE */
VGMSTREAM * init_vgmstream_awb(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t offset, subfile_offset, subfile_next;
    size_t subfile_size;
    int total_subsongs, target_subsong = streamFile->stream_index;
    //uint32_t flags;
    uint16_t alignment, subkey;
    awb_type type;
    char *extension = NULL;


    /* checks
     * .awb: standard
     * .afs2: sometimes [Okami HD (PS4)] */
    if (!check_extensions(streamFile, "awb,afs2"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x41465332) /* "AFS2" */
        goto fail;

    //flags = read_32bitLE(0x08,streamFile);
    total_subsongs = read_32bitLE(0x08,streamFile);
    alignment = (uint16_t)read_16bitLE(0x0c,streamFile);
    subkey    = (uint16_t)read_16bitLE(0x0e,streamFile);

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong > total_subsongs || total_subsongs <= 0) goto fail;

    offset = 0x10;

    /* id(?) table: skip */
    offset += total_subsongs * 0x02;

    /* offset table: find target
     * offset are absolute but sometimes misaligned (specially first that just points to offset table end) */
    {
        off_t file_size = get_streamfile_size(streamFile);
        offset += (target_subsong-1) * 0x04;

        /* last offset is always file end, so table entries = total_subsongs+1 */
        subfile_offset  = read_32bitLE(offset+0x00,streamFile);
        subfile_next    = read_32bitLE(offset+0x04,streamFile);

        subfile_offset += (subfile_offset % alignment) ?
                alignment - (subfile_offset % alignment) : 0;
        subfile_next   += (subfile_next % alignment) && subfile_next < file_size ?
                alignment - (subfile_next % alignment) : 0;
        subfile_size = subfile_next - subfile_offset;

        //todo: flags & 0x200 are uint16 offsets?
    }

    //;VGM_LOG("TXTH: subfile offset=%lx + %x\n", subfile_offset, subfile_size);

    /* autodetect as there isn't anything, plus can mix types
     * (waveid<>codec info is usually in the companion .acb) */
    if ((uint16_t)read_16bitBE(subfile_offset, streamFile) == 0x8000) { /* ADX id */
        type = ADX;
        extension = "adx";
    }
    else if (((uint32_t)read_32bitBE(subfile_offset,streamFile) & 0x7f7f7f7f) == 0x48434100) { /* "HCA\0" */
        type = HCA;
        extension = "hca";
    }
    else if (read_32bitBE(subfile_offset,streamFile) == 0x52494646) { /* "RIFF" */
        type = AT9;
        extension = "at9";
    }
    else if (read_32bitBE(subfile_offset,streamFile) == 0x56414770) { /* "VAGp" */
        type = VAG;
        extension = "vag";
    }
    else {
        goto fail;
    }


    temp_streamFile = setup_subfile_streamfile(streamFile, subfile_offset,subfile_size, extension);
    if (!temp_streamFile) goto fail;

    switch(type) {
        case HCA: /* most common */
            vgmstream = init_vgmstream_hca_subkey(temp_streamFile, subkey);
            if (!vgmstream) goto fail;
            break;
        case ADX: /* Okami HD (PS4) */
            vgmstream = init_vgmstream_adx(temp_streamFile);
            if (!vgmstream) goto fail;
            break;
        case AT9: /* Ukiyo no Roushi (Vita) */
            vgmstream = init_vgmstream_riff(temp_streamFile);
            if (!vgmstream) goto fail;
            break;
        case VAG: /* Ukiyo no Roushi (Vita) */
            vgmstream = init_vgmstream_vag(temp_streamFile);
            if (!vgmstream) goto fail;
            break;
        default:
            goto fail;
    }

    //todo: could try to get name in .acb for this waveid

    vgmstream->num_streams = total_subsongs;

    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
