#include "meta.h"
#include "../coding/coding.h"
#include "ubi_ckd_cwav_streamfile.h"

/* CKD RIFF - UbiArt Framework (v1) audio container [Rayman Origins (3DS)] */
VGMSTREAM* init_vgmstream_ubi_ckd_cwav(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;

    /* checks */
    if (!is_id32be(0x00,sf, "RIFF"))
        goto fail;
    if (!is_id32be(0x08,sf, "WAVE"))
        goto fail;

    if (!(is_id32be(0x0c,sf, "dsph") || is_id32be(0x0c,sf, "cwav")))
        goto fail;

    /* .wav: main (unlike .wav.cdk of other versions) */
    if (!check_extensions(sf,"wav,lwav"))
        goto fail;

    /* inside dsph (header+start, optional) and cwav (body, always) RIFF chunks is a full "CWAV",
     * since dsph also contains some data just deblock */

    temp_sf = setup_ubi_ckd_cwav_streamfile(sf);
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_bcwav(temp_sf);
    if (!vgmstream) goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
