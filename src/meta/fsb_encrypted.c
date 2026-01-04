#include "meta.h"
#include "../util/companion_files.h"
#include "fsb_keys.h"
#include "fsb_encrypted_streamfile.h"


static VGMSTREAM* test_fsbkey(STREAMFILE* sf, const uint8_t* key, size_t key_size, uint8_t flags);

/* fully encrypted FSBs */
VGMSTREAM* init_vgmstream_fsb_encrypted(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;

    /* ignore non-encrypted FSB */
    if ((read_u32be(0x00,sf) & 0xFFFFFF00) == get_id32be("FSB\0"))
        return NULL;

    /* checks */
    /* .fsb: standard
     * .fsb.ps3: various Guitar Hero (PS3)
     * .fsb.xen: various Guitar Hero (X360/PC) */
    if (!check_extensions(sf, "fsb,ps3,xen"))
        return NULL;

    /* try fsbkey + all combinations of FSB4/5 and decryption algorithms */
    {
        uint8_t key[FSB_KEY_MAX];
        size_t key_size = read_key_file(key, FSB_KEY_MAX, sf);

        if (key_size) {
            vgmstream = test_fsbkey(sf, key, key_size, MODE_FSBS);
            return vgmstream;
        }
    }


    /* try all keys until one works */
    if (!vgmstream) {
        for (int i = 0; i < fsbkey_list_count; i++) {
            fsbkey_info entry = fsbkey_list[i];

            vgmstream = test_fsbkey(sf, (const uint8_t*)entry.key, entry.key_size, entry.flags);
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

static VGMSTREAM* test_fsbkey(STREAMFILE* sf, const uint8_t* key, size_t key_size, uint8_t flags) {
    STREAMFILE* temp_sf = NULL;
    VGMSTREAM* vc = NULL;

    if (!key_size)
        return NULL;

    bool test_fsb4 = flags & FLAG_FSB4;
    bool test_fsb5 = flags & FLAG_FSB5;
    bool test_alt  = flags & FLAG_ALT;
    bool test_std  = flags & FLAG_STD;

    if (!vc && test_std && test_fsb_streamfile(sf, key, key_size, 0)) {
        temp_sf = setup_fsb_streamfile(sf, key, key_size, 0);
        if (!temp_sf) return NULL;
        //;dump_streamfile(temp_sf, 0);

        if (!vc && test_fsb4) vc = init_vgmstream_fsb(temp_sf);
        if (!vc && test_fsb5) vc = init_vgmstream_fsb5(temp_sf);

       close_streamfile(temp_sf);
    }

    if (!vc && test_alt && test_fsb_streamfile(sf, key, key_size, 1)) {
        temp_sf = setup_fsb_streamfile(sf, key, key_size, 1);
        if (!temp_sf) return NULL;
        //;dump_streamfile(temp_sf, 1);

        if (!vc && test_fsb4) vc = init_vgmstream_fsb(temp_sf);
        if (!vc && test_fsb5) vc = init_vgmstream_fsb5(temp_sf);

        close_streamfile(temp_sf);
    }
    
    return vc;
}
