#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/endianness.h"


/* EA Harmony Sample Bank - used in 8th gen EA Sports games */
VGMSTREAM* init_vgmstream_ea_sbr_harmony(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE *sf_sbs = NULL, *sf_data = NULL;
    uint64_t base_offset, sound_offset, offset, prev_offset;
    uint32_t dset_id, dset_offset, num_values, num_fields, field_id,
        data_offset, table_offset, set_sounds, sound_table_offset;
    int16_t flag;
    uint16_t num_dsets;
    uint8_t set_type, offset_size;
    char sound_name[STREAM_NAME_SIZE];
    int target_stream = sf->stream_index, total_sounds, local_target, is_streamed = 0;
    int i, j;
    read_u64_t read_u64;
    read_u32_t read_u32;
    read_u16_t read_u16;
    eaac_meta_t info = {0};


    /* checks */
    if (is_id32be(0x00, sf, "SBle")) {
        read_u64 = read_u64le;
        read_u32 = read_u32le;
        read_u16 = read_u16le;
    }
    else if (is_id32be(0x00, sf, "SBbe")) {
        read_u64 = read_u64be;
        read_u32 = read_u32be;
        read_u16 = read_u16be;
    }
    else {
        return NULL;
    }

    if (!check_extensions(sf, "sbr"))
        return NULL;

    num_dsets = read_u16(0x0a, sf);
    table_offset = read_u32(0x18, sf);
    data_offset = read_u32(0x20, sf);

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0)
        goto fail;

    total_sounds = 0;
    sound_offset = 0;

    /* the bank is split into DSET sections each of which references one or multiple sounds */
    /* each set can contain RAM sounds (stored in SBR in data section) or streamed sounds (stored separately in SBS file) */
    for (i = 0; i < num_dsets; i++) {
        dset_offset = read_u32(table_offset + 0x08 * i, sf);
        if (read_u32(dset_offset, sf) != get_id32be("DSET"))
            goto fail;

        dset_id = read_u32(dset_offset + 0x08, sf);
        num_values = read_u32(dset_offset + 0x38, sf);
        num_fields = read_u32(dset_offset + 0x3c, sf);
        local_target = target_stream - total_sounds - 1;
        dset_offset += 0x48;

        /* find RAM or OFF field */
        for (j = 0; j < num_fields; j++) {
            field_id = read_u32(dset_offset, sf);
            if (field_id == get_id32be(".RAM") ||
                field_id == get_id32be(".OFF")) {
                break;
            }

            dset_offset += 0x18;
        }

        if (j == num_fields)
            goto fail;

        /* different set types store offsets differently */
        set_type = read_u8(dset_offset + 0x05, sf);

        /* data sets often contain duplicate offets, need to filter them out however we can */
        /* offsets are stored in ascending order which makes things easier */
        if (set_type == 0x00) {
            set_sounds = 1;
            total_sounds += set_sounds;
            if (local_target < 0 || local_target > 0)
                continue;

            sound_offset = read_u64(dset_offset + 0x08, sf);
        }
        else if (set_type == 0x01) {
            flag = (int16_t)read_u16(dset_offset + 0x06, sf);
            base_offset = read_u64(dset_offset + 0x08, sf);

            set_sounds = num_values;
            total_sounds += set_sounds;
            if (local_target < 0 || local_target >= set_sounds)
                continue;

            sound_offset = base_offset + flag * local_target;
        }
        else if (set_type == 0x02) {
            flag = (read_u16(dset_offset + 0x06, sf) >> 0) & 0xFF;
            offset_size = (read_u16(dset_offset + 0x06, sf) >> 8) & 0xFF;
            base_offset = read_u64(dset_offset + 0x08, sf);
            sound_table_offset = read_u32(dset_offset + 0x10, sf);

            set_sounds = 0;
            prev_offset = UINT64_MAX;
            for (j = 0; j < num_values; j++) {
                if (offset_size == 0x01) {
                    offset = read_u8(sound_table_offset + 0x01 * j, sf);
                }
                else if (offset_size == 0x02) {
                    offset = read_u16(sound_table_offset + 0x02 * j, sf);
                }
                else if (offset_size == 0x04) {
                    offset = read_u32(sound_table_offset + 0x04 * j, sf);
                }
                else {
                    goto fail;
                }
                offset <<= flag;
                offset += base_offset;

                if (offset != prev_offset) {
                    if (set_sounds == local_target)
                        sound_offset = offset;
                    set_sounds++;
                }
                prev_offset = offset;
            }

            total_sounds += set_sounds;
            if (local_target < 0 || local_target >= set_sounds)
                continue;
        }
        else if (set_type == 0x03) {
            offset_size = (read_u16(dset_offset + 0x06, sf) >> 8) & 0xFF;
            set_sounds = read_u64(dset_offset + 0x08, sf);
            sound_table_offset = read_u32(dset_offset + 0x10, sf);

            total_sounds += set_sounds;
            if (local_target < 0 || local_target >= set_sounds)
                continue;

            if (offset_size == 0x01) {
                sound_offset = read_u8(sound_table_offset + 0x01 * local_target, sf);
            }
            else if (offset_size == 0x02) {
                sound_offset = read_u16(sound_table_offset + 0x02 * local_target, sf);
            }
            else if (offset_size == 0x04) {
                sound_offset = read_u32(sound_table_offset + 0x04 * local_target, sf);
            }
            else {
                goto fail;
            }
        }
        else if (set_type == 0x04) {
            sound_table_offset = read_u32(dset_offset + 0x10, sf);

            set_sounds = 0;
            prev_offset = UINT64_MAX;
            for (j = 0; j < num_values; j++) {
                offset = read_u64(sound_table_offset + 0x08 * j, sf);

                if (sound_offset != prev_offset) {
                    if (set_sounds == local_target)
                        sound_offset = offset;
                    set_sounds++;
                }
                prev_offset = offset;
            }

            total_sounds += set_sounds;
            if (local_target < 0 || local_target >= set_sounds)
                continue;
        }
        else {
            goto fail;
        }

        snprintf(sound_name, STREAM_NAME_SIZE, "DSET %08x/%04d", dset_id, local_target);

        if (field_id == get_id32be(".RAM")) {
            is_streamed = 0;
        }
        else if (field_id == get_id32be(".OFF")) {
            is_streamed = 1;
        }
    }

    if (sound_offset == 0)
        goto fail;

    if (!is_streamed) {
        /* RAM asset */
        if (!is_id32be(data_offset, sf, "data") &&
            !is_id32be(data_offset, sf, "DATA"))
            goto fail;

        sf_data = sf;
        sound_offset += data_offset;
    }
    else {
        /* streamed asset */
        sf_sbs = open_streamfile_by_ext(sf, "sbs");
        if (!sf_sbs) goto fail;

        if (!is_id32be(0x00, sf_sbs, "data") &&
            !is_id32be(0x00, sf_sbs, "DATA"))
            goto fail;

        sf_data = sf_sbs;

        if (is_id32be(sound_offset, sf_data, "slot")) {
            /* skip "slot" section */
            sound_offset += 0x30;
        }
    }

    info.sf_head = sf_data;
    info.head_offset = sound_offset;
    info.body_offset = 0x00;
    info.type = meta_EA_SPS;

    vgmstream = load_vgmstream_ea_eaac(&info);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_sounds;
    strncpy(vgmstream->stream_name, sound_name, STREAM_NAME_SIZE);

    close_streamfile(sf_sbs);
    return vgmstream;
fail:
    close_streamfile(sf_sbs);
    return NULL;
}
