#include "meta.h"
#include "../coding/coding.h"

#define THQ_AUS_PS2P_COUNT_OFFSET       0x14
#define THQ_AUS_PS2P_TABLE_OFFSET       0x20
#define THQ_AUS_PS2P_ENTRY_SIZE         0x0C

/* THQ Australia - PS2P [Jimmy Neutron: Attack of the Twonkies (PS2), SpongeBob: Lights, Camera, Pants! (PS2)] */
VGMSTREAM* init_vgmstream_thq_aus_ps2p(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t subfile_offset, subfile_size, total_entries;
    int total_subsongs, target_subsong = sf->stream_index;

    if (!check_extensions(sf, "sounds"))
        return NULL;

    if (!is_id32be(0x00, sf, "ps2p"))
        return NULL;

    /* Read total table entries (Little Endian) */
    total_entries = read_u32le(THQ_AUS_PS2P_COUNT_OFFSET, sf);

    /* The file table has an "off-by-one" structure.
     * The last entry is a terminator that holds the size of the previous file.
     * Therefore, the actual number of playable audio files is (entries - 1). */
    if (total_entries < 2) return NULL;
    total_subsongs = total_entries - 1;

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs) return NULL;

    /* The file table starts at 0x20.
     * Entries are 12 bytes long (0x0C).
     *
     * Structure quirk ("Rotated Table"):
     * Entry[i].Offset = Offset of File[i]
     * Entry[i].Size   = Size of File[i-1] (Previous file)
     *
     * To get the size of File[i], read the Size field from Entry[i+1].
     */

    subfile_offset = read_u32le(THQ_AUS_PS2P_TABLE_OFFSET + (target_subsong - 1) * THQ_AUS_PS2P_ENTRY_SIZE + 0x08, sf);
    subfile_size   = read_u32le(THQ_AUS_PS2P_TABLE_OFFSET + (target_subsong) * THQ_AUS_PS2P_ENTRY_SIZE, sf);

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "vag");
    if (!temp_sf) return NULL;

    vgmstream = init_vgmstream_vag(temp_sf);

    if (vgmstream) {
        vgmstream->num_streams = total_subsongs;
        vgmstream->meta_type = meta_THQ_AUS_PS2P;
        //TODO: Implement full names from name table. vag names capped. Conflicts with duplicate names for different channels.
        vgmstream->stream_name[0] = '\0';
    }

    close_streamfile(temp_sf);
    return vgmstream;
}
