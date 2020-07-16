#include "meta.h"
#include "../coding/coding.h"

/* ADP - from Konami Viper arcade games [ParaParaParadise 2ndMIX (AC)] */
VGMSTREAM* init_vgmstream_adp_konami(STREAMFILE* sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;
    size_t data_size, file_size;


    /* checks */
    if (!check_extensions(sf, "adp"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x41445002) /* "ADP\2" */
        goto fail;

    start_offset = 0x10;
    channels = 2; /* probably @0x03 */
    loop_flag  = 0;

    data_size = read_u32be(0x04,sf);
    file_size = get_streamfile_size(sf);
    if (!(data_size + start_offset - 0x04 <= file_size &&
          data_size + start_offset + 0x04 >= file_size)) /* 1 byte padding in some files */
        goto fail;

    if (read_u32be(0x08,sf) != 0 || read_u32be(0x0c,sf) != 0) /* maybe reserved for loop points */
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_ADP_KONAMI;
    vgmstream->sample_rate = 44100;

    vgmstream->num_samples = oki_bytes_to_samples(data_size, channels);

    vgmstream->coding_type = coding_OKI4S;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
