#include "meta.h"
#include "../coding/coding.h"

static void read_xps_name(VGMSTREAM *vgmstream, STREAMFILE *streamFile, int file_id);

/* .XPS+DAT - From Software games streams [Metal Wolf Chaos (Xbox), Otogi (Xbox)] */
VGMSTREAM* init_vgmstream_xps_dat(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_data = NULL;
    off_t start_offset, header_offset;
    size_t stream_size;
    int loop_flag, channels, sample_rate, codec, loop_start_sample, loop_end_sample, file_id;


    /* checks */
    if (read_u32le(0x00,sf) != get_streamfile_size(sf))
        return NULL;
    if (!check_extensions(sf, "xps"))
        return NULL;

    // 04: bank subsongs
    if (read_u32le(0x08,sf) != 0x02) // type 2=xps
        return NULL;
    if (!is_id32be(0x0c,sf, "diff"))
        return NULL;

    // handle .xps+dat (bank .xps are done below)
    sf_data = open_streamfile_by_ext(sf, "dat");
    if (!sf_data) return NULL;

    // 00: approximate file size
    // 04: subsongs
    // 08: type 1=dat

    int target_subsong = sf->stream_index;
    int total_subsongs = read_s32le(0x04,sf_data);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) return NULL;

    header_offset = 0x20 + 0x94 * (target_subsong-1); // could start at 0x0c or 0x10 too

    file_id             = read_s32le(header_offset+0x00,sf_data);
    start_offset        = read_u32le(header_offset+0x04,sf_data);
    stream_size         = read_u32le(header_offset+0x08,sf_data);
    /* 0x0c: loop start offset? */
    /* 0x10: loop end offset? */
    /* 0x14: always null? */
    codec               = read_u16le(header_offset+0x18,sf_data);
    channels            = read_u16le(header_offset+0x1a,sf_data);
    sample_rate         = read_s32le(header_offset+0x1c,sf_data);
    /* 0x20: average bitrate */
    /* 0x24: block size, bps */
    loop_flag           = read_s32le(header_offset+0x5c,sf_data);
    loop_start_sample   = read_s32le(header_offset+0x6c,sf_data);
    loop_end_sample     = read_s32le(header_offset+0x70,sf_data) + 1; // a "smpl" chunk basically


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->meta_type = meta_XPS_DAT;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    switch(codec) {
        case 0x01:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            vgmstream->num_samples = pcm_bytes_to_samples(stream_size, channels, 16);
            break;

        case 0x69:
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = xbox_ima_bytes_to_samples(stream_size, channels);
            break;

        default:
            goto fail;
    }

    read_xps_name(vgmstream, sf, file_id);

    if (!vgmstream_open_stream(vgmstream,sf_data,start_offset))
        goto fail;

    close_streamfile(sf_data);
    return vgmstream;

fail:
    close_streamfile(sf_data);
    close_vgmstream(vgmstream);
    return NULL;
}

static void read_xps_name(VGMSTREAM* vgmstream, STREAMFILE* sf, int file_id) {
    int name_id = -1;

    /* main section + stream sections (usually same number but not always) */
    int entries = read_s32le(0x04,sf);

    /* "sid\0" entries: find name_id of file_id */
    uint32_t entry_offset = 0x10;
    for (int i = 0; i < entries; i++) {
        uint32_t entry_base  = entry_offset;
        uint32_t entry_size = read_u32le(entry_base+0x00,sf);
        uint32_t entry_id   = read_u32be(entry_base+0x04,sf);
        uint32_t entry_pad  = read_u32le(entry_base+0x08,sf);
        /* 0x0c: always null, rest: entry (format varies) */

        entry_offset += entry_size + entry_pad + 0x10;

        /* sound info entry */
        if (entry_id == get_id32be("sid\0")) {
            int entry_file_id = read_s32le(entry_base+0x10,sf);
            int entry_name_id = read_s32le(entry_base+0x14,sf);
            if (entry_file_id == file_id && name_id == -1) {
                name_id = entry_name_id;
            }
            continue;
        }

        /* sound stream entry, otherwise no good */
        if (entry_id != get_id32be("udss")) {
            return;
        }

        int udss_name_id = read_s32le(entry_base+0x10,sf);
        if (udss_name_id == name_id) {
            off_t name_offset = entry_base + 0x10 + 0x08;
            size_t name_size = entry_size - 0x08; /* includes null */
            read_string(vgmstream->stream_name,name_size, name_offset,sf);
            return;
        }
    }
}

/* .XPS - From Software games banks [Metal Wolf Chaos (Xbox), Otogi (Xbox)] */
VGMSTREAM * init_vgmstream_xps(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_data = NULL;


    /* checks */
    if (read_u32le(0x00,sf) != get_streamfile_size(sf))
        return NULL;
    if (!check_extensions(sf, "xps"))
        return NULL;

    // 04: bank subsongs
    if (read_u32le(0x08,sf) != 0x02) // 2=.xps, 1=.dat
        return NULL;
    if (!is_id32be(0x0c,sf, "diff"))
        return NULL;

    /* handle .xps alone (stream .xps+data are done above) */
    sf_data = open_streamfile_by_ext(sf, "dat");
    if (sf_data) return NULL;

    /* main section + bank sections (usually same number but not always) */
    int entries = read_s32le(0x04,sf);

    int target_subsong = sf->stream_index;
    int total_subsongs = 0;
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 /*|| target_subsong > total_subsongs || total_subsongs < 1*/)
        return NULL;

    //TODO: should probably unify
    // - there is one "sid" per subsong with base config and one "udsb" per memory or external .dat stream
    // - each "sid" is linked to a udsb by internal+external ids
    // - however sids don't seem to be defined in the same order as streams
    // - streams in .dat also don't seem to follow the sid order

    /* parse entries */
    uint32_t entry_offset = 0x10;
    for (int i = 0; i < entries; i++) {
        uint32_t entry_base = entry_offset;
        uint32_t entry_size = read_u32le(entry_base+0x00,sf);
        uint32_t entry_id   = read_u32be(entry_base+0x04,sf);
        uint32_t entry_pad  = read_u32le(entry_base+0x08,sf);
        // 0c: null or garbage from other entries
        // rest: entry (format varies)

        entry_offset += entry_size + entry_pad + 0x10;

        // sound info entry
        if (entry_id == get_id32be("sid\0")) {
            // 10: external id
            // 14: internal id
            // 30: external flag

            /* keep looking for stream entries */
            continue;
        }

        // stream entry
        if (entry_id != get_id32be("udsb")) {
            goto fail;
        }



        total_subsongs++;

        /* open internal RIFF */
        if (target_subsong == total_subsongs && vgmstream == NULL) {
            STREAMFILE* temp_sf;
            // 10: internal id
            uint32_t subsong_size  = read_u32le(entry_base + 0x14,sf);
            uint32_t subsong_offset = entry_base + 0x18;

            temp_sf = setup_subfile_streamfile(sf, subsong_offset, subsong_size, "wav");
            if (!temp_sf) goto fail;

            vgmstream = init_vgmstream_riff(temp_sf);
            close_streamfile(temp_sf);
            if (!vgmstream) goto fail;
        }
    }

    /* subsong not found */
    if (!vgmstream)
        goto fail;
    vgmstream->num_streams = total_subsongs;
    return vgmstream;
fail:
    close_streamfile(sf_data);
    close_vgmstream(vgmstream);
    return NULL;
}
