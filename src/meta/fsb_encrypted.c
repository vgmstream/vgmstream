#include "meta.h"
#include "fsb_keys.h"
#include "fsb_encrypted_streamfile.h"


/* fully encrypted FSBs */
VGMSTREAM* init_vgmstream_fsb_encrypted(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;

    /* ignore non-encrypted FSB */
    if ((read_u32be(0x00,sf) & 0xFFFFFF00) == get_id32be("FSB\0"))
        goto fail;

    /* checks */
    /* .fsb: standard
     * .fsb.ps3: various Guitar Hero (PS3)
     * .fsb.xen: various Guitar Hero (X360/PC) */
    if (!check_extensions(sf, "fsb,ps3,xen"))
        goto fail;


    /* try fsbkey + all combinations of FSB4/5 and decryption algorithms */
    {
        STREAMFILE* temp_sf = NULL;
        uint8_t key[FSB_KEY_MAX];
        size_t key_size = read_key_file(key, FSB_KEY_MAX, sf);

        if (key_size) {
            {
                temp_sf = setup_fsb_streamfile(sf, key,key_size, 0);
                if (!temp_sf) goto fail;

                if (!vgmstream) vgmstream = init_vgmstream_fsb(temp_sf);
                if (!vgmstream) vgmstream = init_vgmstream_fsb5(temp_sf);

                close_streamfile(temp_sf);
            }

            if (!vgmstream) {
                temp_sf = setup_fsb_streamfile(sf, key,key_size, 1);
                if (!temp_sf) goto fail;

                if (!vgmstream) vgmstream = init_vgmstream_fsb(temp_sf);
                if (!vgmstream) vgmstream = init_vgmstream_fsb5(temp_sf);

                close_streamfile(temp_sf);
            }
        }
    }


    /* try all keys until one works */
    if (!vgmstream) {
        int i;
        STREAMFILE* temp_sf = NULL;

        for (i = 0; i < fsbkey_list_count; i++) {
            fsbkey_info entry = fsbkey_list[i];
            //;VGM_LOG("fsbkey: size=%i, is_fsb5=%i, is_alt=%i\n", entry.fsbkey_size,entry.is_fsb5, entry.is_alt);

            temp_sf = setup_fsb_streamfile(sf, entry.fsbkey, entry.fsbkey_size, entry.is_alt);
            if (!temp_sf) goto fail;

            if (fsbkey_list[i].is_fsb5) {
                vgmstream = init_vgmstream_fsb5(temp_sf);
            } else {
                vgmstream = init_vgmstream_fsb(temp_sf);
            }

            //;if (vgmstream) dump_streamfile(temp_sf, 0);

            close_streamfile(temp_sf);
            if (vgmstream) break;
        }
    }

    if (!vgmstream)
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
