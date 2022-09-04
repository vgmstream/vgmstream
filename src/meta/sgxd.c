#include "meta.h"
#include "../coding/coding.h"


/* SGXD - Sony/SCEI's format (SGB+SGH / SGD / SGX) */
VGMSTREAM* init_vgmstream_sgxd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_head = NULL;
    STREAMFILE* sf_body = NULL;
    off_t start_offset, data_offset, chunk_offset, name_offset = 0;
    size_t stream_size;
    uint32_t base1_offset, base2_offset, base3_offset;

    int is_sgx, is_sgd = 0;
    int loop_flag, channels, codec, sample_rate;
    int32_t num_samples, loop_start_sample, loop_end_sample;
    int total_subsongs, target_subsong = sf->stream_index;


    /* for plugins that start with .sgb */
    if (check_extensions(sf,"sgb")) {
        sf_head = open_streamfile_by_ext(sf, "sgh");
        if (!sf_head) goto fail;
    }
    else {
        sf_head = sf;
    }

    if (!is_id32be(0x00,sf_head, "SGXD"))
        goto fail;

    /* checks */
    /* .sgx: header+data (Genji)
     * .sgd: header+data (common)
     * .sgh+sgd: header+data (streams) */
    if (!check_extensions(sf,"sgx,sgd,sgb"))
        goto fail;

    /* SGXD base (size 0x10), always LE even on PS3 */
    /* 0x04: SGX = full header size
             SGD/SGH = bank name offset (part of NAME table, usually same as filename) */
    /* 0x08: SGX = first chunk offset? (0x10) 
             SGD/SGH = full header size */
    /* 0x0c: SGX/SGH = full data size with padding / 
             SGD = full data size ^ (1<<31) with padding */
    base1_offset = read_u32le(0x04, sf_head);
    base2_offset = read_u32le(0x08, sf_head);
    base3_offset = read_u32le(0x0c, sf_head);

    is_sgx = base2_offset == 0x10; /* fixed size */
    is_sgd = base3_offset & (1 << 31); /* flag */

    /* Ogg SGXD don't have flag (probably due to codec hijack, or should be split), allow since it's not so obvious */
    if (!(is_sgx || is_sgd) && get_streamfile_size(sf_head) != base2_offset) /* sgh but wrong header size must be sgd */
        is_sgd = 1;

    /* for plugins that start with .sgh (and don't check extensions) */
    if (!(is_sgx || is_sgd) && sf == sf_head) {
        sf_body = open_streamfile_by_ext(sf, "sgb");
        if (!sf_body) goto fail;
    }
    else {
        sf_body = sf;
    }


    if (is_sgx) {
        data_offset = base1_offset;
    } else if (is_sgd) {
        data_offset = base2_offset;
    } else {
        data_offset = 0x00;
    }


    /* Format per chunk:
     * - 0x00: id
     * - 0x04: SGX: unknown; SGD/SGH: chunk length
     * - 0x08: null
     * - 0x0c: entries */

    /* typical chunks (with some entry info):
     * - WAVE: wave data (see below)
     * - RGND: programs info, notably:
     *   - 0x18: min note range
     *   - 0x19: max note range
     *   - 0x1C: root note 
     *   - 0x1d: fine tuning
     *   - 0x34: WAVE id
     *   > sample_rate = wave_sample_rate * (2 ^ (1/12)) ^ (target_note - root_note)
     * - NAME: strings for other chunks
     *   - 0x00: sub-id?
     *   - 0x02: type? (possibly: 0000=bank, 0x2xxx=SEQD/WAVE, 0x3xxx=WSUR, 0x4xxx=BUSS, 0x6xxx=CONF)
     *   - 0x04: absolute offset
     * - SEQD: related to SFX (sequences?), entries seem to be offsets to name offset + sequence offset
     *   > sequence format seems to be 1 byte type (0=sfx, 1=music) + midi without header
     *     (default tick resolution of 960 pulses per quarter note). They use Midi Time Code
     *     (like 30fps with around 196 ticks per frame), and same controller event for looping as old SEQs (CC 99).
     * - WSUR: ?
     * - WMKR: ?
     * - CONF: ? (name offset + config offset)
     * - BUSS: bus config? */

    /* WAVE chunk (size 0x10 + files * 0x38 + optional padding) */
    if (is_sgx) { /* position after chunk+size */
        if (!is_id32be(0x10,sf_head, "WAVE"))
            goto fail;
        chunk_offset = 0x18;
    } else {
        if (!find_chunk_le(sf_head, get_id32be("WAVE"),0x10,0, &chunk_offset, NULL))
            goto fail;
    }

    /* check multi-streams (usually only SE containers; Puppeteer) */
    total_subsongs = read_s32le(chunk_offset+0x04,sf_head);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    /* read stream header */
    {
        uint32_t stream_offset;
        chunk_offset += 0x08 + 0x38 * (target_subsong-1); /* position in target header*/

        /* 0x00: ? (00/01/02) */
        if (!is_sgx) /* meaning unknown in .sgx; offset 0 = not a stream (a RGND sample) */
            name_offset = read_u32le(chunk_offset+0x04,sf_head);
        codec = read_u8(chunk_offset+0x08,sf_head);
        channels = read_u8(chunk_offset+0x09,sf_head);
        /* 0x0a: null */
        sample_rate = read_s32le(chunk_offset+0x0c,sf_head);

        /* 0x10: info_type, meaning of the next value
         *       (00=null, 30/40=data size without padding (ADPCM, ATRAC3plus), 80/A0=block size (AC3) */
        /* 0x14: info_value (see above) */
        /* 0x18: unknown (ex. 0x0008/0010/3307/CC02/etc, RGND related?) x2 */
        /* 0x1c: null */

        num_samples = read_s32le(chunk_offset+0x20,sf_head);
        loop_start_sample = read_s32le(chunk_offset+0x24,sf_head);
        loop_end_sample = read_s32le(chunk_offset+0x28,sf_head);
        stream_size = read_u32le(chunk_offset+0x2c,sf_head); /* stream size (without padding) / interleave (for type3) */

        if (is_sgx) {
            stream_offset = 0x0;
        } else{
            stream_offset = read_u32le(chunk_offset+0x30,sf_head);
        }
        /* 0x34: SGX = unknown
         *       SGD/SGH = stream size (with padding) / interleave */

        loop_flag = loop_start_sample != -1 && loop_end_sample != -1;
        start_offset = data_offset + stream_offset;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_SGXD;
    if (name_offset)
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,sf_head);

    switch (codec) {

        case 0x01:      /* PCM [LocoRoco Cocoreccho! (PS3)] (rare, locoloco_psn#279) */
            vgmstream->coding_type = coding_PCM16BE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;

#ifdef VGM_USE_VORBIS
        case 0x02:      /* Ogg Vorbis [Ni no Kuni: Wrath of the White Witch Remastered (PC)] (codec hijack?) */
            vgmstream->codec_data = init_ogg_vorbis(sf_body, start_offset, stream_size, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_OGG_VORBIS;
            vgmstream->layout_type = layout_none;
            break;
#endif
        case 0x03:      /* PS-ADPCM [Genji (PS3), Ape Escape Move (PS3)]*/
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            if (!is_sgd) {
                vgmstream->interleave_block_size = 0x10;
            } else { /* this only seems to happen with SFX */
                vgmstream->interleave_block_size = stream_size;
            }

            /* a few files in LocoRoco set 0 stream size/samples, use an empty file for now */
            if (vgmstream->num_samples == 0)
                vgmstream->num_samples = 28;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x04: {    /* ATRAC3plus [Kurohyo 1/2 (PSP), BraveStory (PSP)] */
            vgmstream->codec_data = init_ffmpeg_atrac3_riff(sf_body, start_offset, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* SGXD's sample rate has priority over RIFF's sample rate (may not match) */
            /* loop/sample values are relative (without skip) vs RIFF (with skip), matching "smpl" otherwise */
            break;
        }
#endif
        case 0x05:      /* Short PS-ADPCM [Afrika (PS3), LocoRoco Cocoreccho! (PS3)] */
            vgmstream->coding_type = coding_PSX_cfg;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x4;
            vgmstream->codec_config = 1; /* needs extended table */

            break;

#ifdef VGM_USE_FFMPEG
        case 0x06: {    /* AC3 [Tokyo Jungle (PS3), Afrika (PS3)] */
            vgmstream->codec_data = init_ffmpeg_offset(sf_body, start_offset, stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* PS3 AC3 consistently has 256 encoder delay samples, and there are ~1000-2000 samples after num_samples.
             * Skipping them marginally improves full loops in some Tokyo Jungle tracks (ex. a_1.sgd). */
            ffmpeg_set_skip_samples(vgmstream->codec_data, 256);

            /* SGXD loop/sample values are relative (without skip samples), no need to adjust */
            break;
        }
#endif

        default:
            VGM_LOG("SGDX: unknown codec %i\n", codec);
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf_body, start_offset))
        goto fail;

    if (sf != sf_head) close_streamfile(sf_head);
    if (sf != sf_body) close_streamfile(sf_body);
    return vgmstream;

fail:
    if (sf != sf_head) close_streamfile(sf_head);
    if (sf != sf_body) close_streamfile(sf_body);
    close_vgmstream(vgmstream);
    return NULL;
}
