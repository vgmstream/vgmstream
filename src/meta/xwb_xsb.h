#ifndef _XWB_XSB_H_
#define _XWB_XSB_H_
#include "meta.h"

#define XSB_XACT1_0_MAX 5       /* Unreal Championship (Xbox) */
#define XSB_XACT1_1_MAX 8       /* Die Hard: Vendetta (Xbox) */
#define XSB_XACT1_2_MAX 11      /* other Xbox games */
#define XSB_XACT2_MAX   41      /* other PC/X360 games */


typedef struct {
    /* config */
    int selected_stream;
    int selected_wavebank;

    /* state */
    int big_endian;
    int version;

    int   simple_cues_count;
    off_t simple_cues_offset;
    int   complex_cues_count;
    off_t complex_cues_offset;
    int   sounds_count;
    off_t sounds_offset;
    int   wavebanks_count;
    off_t wavebanks_offset;
    int   wavebanks_name_size;
    off_t nameoffsets_offset;
    int   cue_names_size;
    off_t cue_names_offset;

    /* output */
    int parse_done;
    char name[STREAM_NAME_SIZE];
    int name_len;

} xsb_header;


static void xsb_check_stream(xsb_header * xsb, int stream_index, int wavebank_index, off_t name_offset, STREAMFILE *sf) {
    if (xsb->parse_done)
        return;

    /* multiple names may correspond to a stream (ex. Blue Dragon), so we concat all */
    if (xsb->selected_stream == stream_index &&
            (xsb->selected_wavebank == wavebank_index || wavebank_index == -1 || wavebank_index == 255)) {
        char name[STREAM_NAME_SIZE];
        size_t name_size;

        name_size = read_string(name,sizeof(name), name_offset,sf); /* null-terminated */

        if (xsb->name_len) {
            const char *cat = "; ";
            int cat_len = 2;

            if (xsb->name_len + cat_len + name_size + 1 < STREAM_NAME_SIZE) {
                strcat(xsb->name + xsb->name_len, cat);
                strcat(xsb->name + xsb->name_len, name);
            }
        }
        else {
            strcpy(xsb->name, name);
        }
        xsb->name_len += name_size;
        //xsb->parse_done = 1; /* uncomment this to stop reading after first name */
        //;VGM_LOG("XSB: parse found stream=%i, wavebank=%i, name_offset=%lx\n", stream_index, wavebank_index, name_offset);
    }
}

/* old XACT1 is a bit different and much of it is unknown but this seems to work.
 * - after header is the simple(?) cues table then complex(?) cues table
 * - simple cues point to complex cues by index
 * - complex cues may have stream/wavebank or point again to a sound(?) with the stream/wavebank
 */
static int parse_xsb_cues_old(xsb_header * xsb, STREAMFILE *sf) {
    int32_t  (*read_s32)(off_t,STREAMFILE*) = xsb->big_endian ? read_s32be : read_s32le;
    int16_t  (*read_s16)(off_t,STREAMFILE*) = xsb->big_endian ? read_s16be : read_s16le;

    uint8_t flags, subflags;
    int cue_index, stream_index, wavebank_index = 0;
    off_t offset, name_offset, cue_offset, sound_offset;
    int i;
    size_t simple_entry, complex_entry;


    if (xsb->version <= XSB_XACT1_1_MAX) {
        simple_entry = 0x10;
        complex_entry = 0x14;
    }
    else if (xsb->version <= XSB_XACT1_2_MAX) {
        simple_entry = 0x14;
        complex_entry = 0x14;
    }
    else {
        VGM_LOG("XSB: unknown old format for version %x\n", xsb->version);
        goto fail;
    }


    offset = xsb->sounds_offset;
    for (i = 0; i < xsb->simple_cues_count; i++) {

        /* *** simple sound *** */
        /* 00(2): flags? */
        cue_index   = read_s16(offset + 0x02,sf);
        name_offset = read_s32(offset + 0x04,sf);
        /* 06-14: unknown */

        //;VGM_LOG("XSB old simple at %lx: cue=%i, name_offset=%lx\n", offset, cue_index, name_offset);
        offset += simple_entry;

        /* when cue_index is -1 @0x08 points to some offset (random sound type?) [ex. ATV 3 Lawless (Xbox)] */
        if (cue_index < 0 && cue_index > xsb->complex_cues_count) {
            VGM_LOG("XSB old: ignored cue index %i\n", cue_index);
            continue;
        }


        /* *** complex sound *** */
        cue_offset = xsb->sounds_offset + xsb->simple_cues_count*simple_entry + cue_index*complex_entry;

        /* most fields looks like more flags and optional offsets depending of flags */
        flags = read_u8(cue_offset + 0x0b,sf);

        if (flags & 8) { /* simple */
            stream_index    = read_s16(cue_offset + 0x00,sf);
            wavebank_index  = read_s16(cue_offset + 0x02,sf);
        }
        //else if (flags & 4) { /* unsure */
        //    VGM_LOG("XSB old complex at %lx: unknown flags=%x\n", cue_offset, flags);
        //    continue;
        //}
        else { /* complex (flags none/1/2) */
            sound_offset = read_s32(cue_offset + 0x00,sf);

            /* *** jump entry *** */
            /* 00(1): flags? */
            sound_offset = read_s32(sound_offset + 0x01,sf) & 0x00FFFFFF; /* 24b */

            /* *** sound entry *** */
            subflags = read_u8(sound_offset + 0x00,sf);
            if (subflags == 0x00) { /* 0x0c entry */
                stream_index    = read_s16(sound_offset + 0x08,sf);
                wavebank_index  = read_s16(sound_offset + 0x0a,sf);
            }
            else if (subflags == 0x0a) { /* 0x20 entry */
                stream_index    = read_s16(sound_offset + 0x1c,sf);
                wavebank_index  = read_s16(sound_offset + 0x1e,sf);
            }
            else {
                VGM_LOG("XSB old sound at %lx: unknown subflags=%x\n", sound_offset, subflags);
                continue;
            }
        }

        //;VGM_LOG("XSB old complex at %lx: flags=%x, stream=%i, wavebank=%i, name_offset=%lx\n", cue_offset, flags, stream_index, wavebank_index, name_offset);
        xsb_check_stream(xsb, stream_index, wavebank_index, name_offset,sf);
        if (xsb->parse_done) return 1;
    }

    return 1;
fail:
    return 0;
}

static int parse_xsb_clip(xsb_header * xsb, off_t offset, off_t name_offset, STREAMFILE *sf) {
    uint32_t (*read_u32)(off_t,STREAMFILE*) = xsb->big_endian ? read_u32be : read_u32le;
    int16_t  (*read_s16)(off_t,STREAMFILE*) = xsb->big_endian ? read_s16be : read_s16le;

    uint32_t flags;
    int stream_index, wavebank_index;
    int i, t, track_count, event_count;


    event_count = read_s8(offset + 0x00,sf);

    //;VGM_LOG("XSB clip at %lx\n", offset);
    offset += 0x01;

    for (i = 0; i < event_count; i++) {
        flags = read_u32(offset + 0x00,sf);
        /* 04(2): random offset */

        //;VGM_LOG("XSB clip event: %x at %lx\n", flags, offset);
        offset += 0x06;

        switch (flags & 0x1F) { /* event ID */

            case 0x01: /* playwave event */
                /* 00(1): unknown */
                /* 01(1): flags */
                stream_index    = read_s16(offset + 0x02,sf);
                wavebank_index  = read_s8 (offset + 0x04,sf);
                /* 05(1): loop count */
                /* 06(2): pan angle */
                /* 08(2): pan arc */

                //;VGM_LOG("XSB clip event 1 at %lx: stream=%i, wavebank=%i\n", offset, stream_index, wavebank_index);
                offset += 0x0a;

                xsb_check_stream(xsb, stream_index, wavebank_index, name_offset,sf);
                if (xsb->parse_done) return 1;
                break;

            case 0x03: /* playwave event */
                /* 00(1): unknown */
                /* 01(1): flags */
                /* 02(1): loop count */
                /* 03(2): pan angle */
                /* 05(2): pan arc */
                track_count = read_s16(offset + 0x07,sf);
                /* 09(1): flags? */
                /* 0a(5): unknown */

                //;VGM_LOG("XSB clip event 3 at %lx\n", offset);
                offset += 0x0F;

                for (t = 0; t < track_count; t++) {
                    stream_index    = read_s16(offset + 0x00,sf);
                    wavebank_index  = read_s8 (offset + 0x02,sf);
                    /* 03(1): min weight */
                    /* 04(1): min weight */

                    //;VGM_LOG("XSB clip event 3: track=%i, stream=%i, wavebank=%i\n", t, stream_index, wavebank_index);
                    offset += 0x05;

                    xsb_check_stream(xsb, stream_index, wavebank_index, name_offset,sf);
                    if (xsb->parse_done) return 1;
                }
                break;

            case 0x04: /* playwave event */
                /* 00(1): unknown */
                /* 01(1): flags */
                stream_index    = read_s16(offset + 0x02,sf);
                wavebank_index  = read_s8 (offset + 0x04,sf);
                /* 05(1): loop count */
                /* 06(2): pan angle */
                /* 08(2): pan arc */
                /* 0a(2): min pitch */
                /* 0c(2): max pitch */
                /* 0e(1): min volume */
                /* 0f(1): max volume */
                /* 10(4): min frequency */
                /* 14(4): max frequency */
                /* 18(1): min Q */
                /* 19(1): max Q */
                /* 1a(1): unknown */
                /* 1b(1): variation flags */

                //;VGM_LOG("XSB clip event 4 at %lx: stream=%i, wavebank=%i\n", offset, stream_index, wavebank_index);
                offset += 0x1c;

                xsb_check_stream(xsb, stream_index, wavebank_index, name_offset,sf);
                if (xsb->parse_done) return 1;
                break;

            case 0x06: /* playwave event */
                /* 00(1): unknown */
                /* 01(1): flags */
                /* 02(1): loop count */
                /* 03(2): pan angle */
                /* 05(2): pan arc */
                /* 07(2): min pitch */
                /* 09(2): max pitch */
                /* 0b(1): min volume */
                /* 0c(1): max volume */
                /* 0d(4): min frequency */
                /* 11(4): max frequency */
                /* 15(1): min Q */
                /* 16(1): max Q */
                /* 17(1): unknown */
                /* 18(1): variation flags */
                track_count = read_s16(offset + 0x19,sf);
                /* 1a(1): flags 2 */
                /* 1b(5): unknown 2 */

                //;VGM_LOG("XSB clip event 6 at %lx\n", offset);
                offset += 0x20;

                for (t = 0; t < track_count; t++) {
                    stream_index    = read_s16(offset + 0x00,sf);
                    wavebank_index  = read_s8 (offset + 0x02,sf);
                    /* 03(1): min weight */
                    /* 04(1): min weight */

                    //;VGM_LOG("XSB clip event 6: track=%i, stream=%i, wavebank=%i at %lx\n", t, stream_index, wavebank_index, offset);
                    offset += 0x05;

                    xsb_check_stream(xsb, stream_index, wavebank_index, name_offset,sf);
                    if (xsb->parse_done) return 1;
                }
                break;

            case 0x08: /* volume event */
                /* 00(2): unknown */
                /* 02(1): flags */
                /* 03(4): decibels */
                /* 07(9): unknown */

                //;VGM_LOG("XSB clip event 8 at %lx\n", offset);
                offset += 0x10;
                break;

            case 0x00: /* stop event */
            case 0x07: /* pitch event */
            case 0x09: /* marker event */
            case 0x11: /* volume repeat event */
            default:
                VGM_LOG("XSB event: unknown type %x at %lx\n", flags, offset);
                goto fail;
        }
    }

    return 1;
fail:
    return 0;
}

static int parse_xsb_sound(xsb_header * xsb, off_t offset, off_t name_offset, STREAMFILE *sf) {
    int32_t  (*read_s32)(off_t,STREAMFILE*) = xsb->big_endian ? read_s32be : read_s32le;
    int16_t  (*read_s16)(off_t,STREAMFILE*) = xsb->big_endian ? read_s16be : read_s16le;

    uint8_t flags;
    int stream_index = 0, wavebank_index = 0;
    int i, clip_count = 0;


    flags = read_u8 (offset + 0x00,sf);
    /* 0x01(2): category */
    /* 0x03(1): decibels */
    /* 0x04(2): pitch */
    /* 0x06(1): priority */
    /* 0x07(2): entry size? "filter stuff"? */

    //;VGM_LOG("XSB sound at %lx\n", offset);
    offset += 0x09;

    if (flags & 0x01) { /* complex sound */
        clip_count      = read_u8 (offset + 0x00,sf);

        //;VGM_LOG("XSB sound: complex with clips=%i\n", clip_count);
        offset += 0x01;
    }
    else {
        stream_index    = read_s16(offset + 0x00,sf);
        wavebank_index  =  read_s8(offset + 0x02,sf);

        //;VGM_LOG("XSB sound: simple with stream=%i, wavebank=%i\n", stream_index, wavebank_index);
        offset += 0x03;

        xsb_check_stream(xsb, stream_index, wavebank_index, name_offset,sf);
        if (xsb->parse_done) return 1;
    }

    if (flags & 0x0E) { /* has RPCs */
        size_t rpc_size = read_s16(offset + 0x00,sf);
        /* 0x02(2): preset count */
        /* 0x04(4*count): RPC indexes */
        /* (presets per flag 2/4/8 flag) */
        offset += rpc_size;
    }

    if (flags & 0x10) { /* has DSPs */
        size_t dsp_size = read_s16(offset + 0x00,sf);
        /* follows RPC format? */
        offset += dsp_size;
    }

    if (flags & 0x01) { /* complex sound clips */
        off_t clip_offset;
        for (i = 0; i < clip_count; i++) {
            /* 00(1): decibels */
            clip_offset = read_s32(offset + 0x01,sf);
            /* 05(2): filter config */
            /* 07(2): filter frequency */

            //;VGM_LOG("XSB sound clip %i at %lx\n", i, offset);
            offset += 0x09;

            parse_xsb_clip(xsb, clip_offset, name_offset,sf);
            if (xsb->parse_done) return 1;
        }
    }

    return 0;
}

static int parse_xsb_variation(xsb_header * xsb, off_t offset, off_t name_offset, STREAMFILE *sf) {
    int32_t  (*read_s32)(off_t,STREAMFILE*) = xsb->big_endian ? read_s32be : read_s32le;
    uint16_t (*read_u16)(off_t,STREAMFILE*) = xsb->big_endian ? read_u16be : read_u16le;
    int16_t  (*read_s16)(off_t,STREAMFILE*) = xsb->big_endian ? read_s16be : read_s16le;

    uint16_t flags;
    int stream_index, wavebank_index;
    int i, variation_count;


    variation_count = read_s16(offset + 0x00,sf);
    flags           = read_u16(offset + 0x02,sf);

    //;VGM_LOG("XSB variation at %lx\n", offset);
    offset += 0x04;

    for (i = 0; i < variation_count; i++) {
        off_t sound_offset;

        switch ((flags >> 3) & 0x7) {
            case 0: /* wave */
                stream_index   = read_s16(offset + 0x00,sf);
                wavebank_index =  read_s8(offset + 0x02,sf);
                /* 03(1): weight min */
                /* 04(1): weight max */

                //;VGM_LOG("XSB variation: type 0 with stream=%i, wavebank=%i\n", stream_index, wavebank_index);
                offset += 0x05;

                xsb_check_stream(xsb, stream_index, wavebank_index, name_offset,sf);
                if (xsb->parse_done) return 1;
                break;

            case 1: /* sound */
                sound_offset = read_s32(offset + 0x00,sf);
                /* 04(1): weight min */
                /* 05(1): weight max */

                //;VGM_LOG("XSB variation: type 1\n");
                offset += 0x06;

                parse_xsb_sound(xsb, sound_offset, name_offset,sf);
                if (xsb->parse_done) return 1;
                break;

            case 3: /* sound */
                sound_offset = read_s32(offset + 0x00,sf);
                /* 04(4): weight min */
                /* 08(4): weight max */
                /* 0c(4): flags */

                //;VGM_LOG("XSB variation: type 3\n");
                offset += 0x10;

                parse_xsb_sound(xsb, sound_offset, name_offset,sf);
                if (xsb->parse_done) return 1;
                break;

            case 4: /* compact wave */
                stream_index   = read_s16(offset + 0x00,sf);
                wavebank_index =  read_s8(offset + 0x02,sf);

                //;VGM_LOG("XSB variation: type 4 with stream=%i, wavebank=%i\n", stream_index, wavebank_index);
                offset += 0x03;

                xsb_check_stream(xsb, stream_index, wavebank_index, name_offset,sf);
                if (xsb->parse_done) return 1;
                break;

            default:
                VGM_LOG("XSB variation: unknown type %x at %lx\n", flags, offset);
                goto fail;
        }
    }

    /* 00(1): unknown */
    /* 01(2): unknown */
    /* 03(1): unknown */
    offset += 0x04;


    return 1;
fail:
    return 0;
}


static int parse_xsb_cues_new(xsb_header * xsb, STREAMFILE *sf) {
    int32_t  (*read_s32)(off_t,STREAMFILE*) = xsb->big_endian ? read_s32be : read_s32le;

    uint8_t flags;
    off_t offset, name_offset, sound_offset;
    off_t names_offset = xsb->nameoffsets_offset;
    int i;


    offset = xsb->simple_cues_offset;
    for (i = 0; i < xsb->simple_cues_count; i++) {
        /* 00(1): flags */
        sound_offset = read_s32(offset + 0x01,sf);

        //;VGM_LOG("XSB cues: simple %i at %lx\n", i, offset);
        offset += 0x05;

        name_offset = read_s32(names_offset + 0x00,sf);
        /* 04(2): unknown (-1) */
        names_offset += 0x06;

        parse_xsb_sound(xsb, sound_offset, name_offset,sf);
        if (xsb->parse_done) break;
    }

    offset = xsb->complex_cues_offset;
    for (i = 0; i < xsb->complex_cues_count; i++) {
        flags = read_u8(offset + 0x00,sf);
        sound_offset = read_s32(offset + 0x01,sf);
        /* 05(4): unknown (sound) / transition table offset (variation) */
        /* 09(1): instance limit */
        /* 0a(2): fade in sec */
        /* 0c(2): fade out sec */
        /* 0e(1): instance flags */

        //;VGM_LOG("XSB cues: complex %i at %lx\n", i, offset);
        offset += 0x0f;

        name_offset = read_s32(names_offset + 0x00,sf);
        /* 04(2): unknown (-1) */
        names_offset += 0x06;

        if (flags & (1<<2))
            parse_xsb_sound(xsb, sound_offset, name_offset,sf);
        else
            parse_xsb_variation(xsb, sound_offset, name_offset,sf);
        if (xsb->parse_done) break;
    }

    return 1;
}

/**
 * XWB "wave bank" has streams (channels, loops, etc), while XSB "sound bank" has cues/sounds
 * (volume, pitch, name, etc). Each XSB cue/sound has a variable size and somewhere inside may
 * be the stream/wavebank index (some cues are just commands, though).
 *
 * We want to find a cue pointing to our current wave to get the name. Cues may point to
 * multiple streams out of order, and a stream can be used by multiple cues:
 * - name 1: simple cue 1 > simple sound 2 > xwb stream 3
 * - name 2: simple cue 2 > complex sound 1 > clip 1/2/3 > xwb streams 4/5/5
 * - name 3: complex cue 1 > simple sound 3 > xwb stream 0
 * - name 4: complex cue 2 > variation > xwb stream 1
 * - name 5: complex cue 3 > variation > simple sound 4/5 > xwb streams 0/1
 * - etc
 * Names are optional (almost always included though), and some cues don't have a name
 * even if others do. Some offsets are optional, usually signaled by -1/wrong values.
 *
 * More info:
 * - https://wiki.multimedia.cx/index.php/XACT
 * - https://github.com/MonoGame/MonoGame/blob/master/MonoGame.Framework/Audio/Xact/
 * - https://github.com/espes/MacTerrariaWrapper/tree/master/xactxtract
 */
static int parse_xsb(xsb_header * xsb, STREAMFILE *sf, char *xwb_wavebank_name) {
    int32_t  (*read_s32)(off_t,STREAMFILE*) = NULL;
    int16_t  (*read_s16)(off_t,STREAMFILE*) = NULL;


    /* check header */
    if ((read_u32be(0x00,sf) != 0x5344424B) &&    /* "SDBK" (LE) */
        (read_u32be(0x00,sf) != 0x4B424453))      /* "KBDS" (BE) */
        goto fail;

    xsb->big_endian = (read_u32be(0x00,sf) == 0x4B424453); /* "KBDS" */
    read_s32 = xsb->big_endian ? read_s32be : read_s32le;
    read_s16 = xsb->big_endian ? read_s16be : read_s16le;


    /* parse sound bank header */
    xsb->version = read_s16(0x04,sf); /* tool version */
    if (xsb->version <= XSB_XACT1_0_MAX) {
        /* 06(2): crc */
        xsb->wavebanks_offset       = read_s32(0x08,sf);
        /* 0c(4): unknown1 offset (entry: 0x04) */
        /* 10(4): unknown2 offset */
        /* 14(2): element count? */
        /* 16(2): empty? */
        /* 18(2): empty? */
        xsb->complex_cues_count     = read_s16(0x1a,sf);
        xsb->simple_cues_count      = read_s16(0x1c,sf);
        xsb->wavebanks_count        = read_s16(0x1e,sf);
        /* 20(10): xsb name */

        xsb->sounds_offset          = 0x30;
        xsb->wavebanks_name_size    = 0x10;
    }
    else if (xsb->version <= XSB_XACT1_1_MAX) {
        /* 06(2): crc */
        xsb->wavebanks_offset       = read_s32(0x08,sf);
        /* 0c(4): unknown1 offset (entry: 0x04) */
        /* 10(4): unknown2 offset */
        /* 14(4): unknown3 offset */
        /* 18(2): empty? */
        /* 1a(2): element count? */
        xsb->complex_cues_count     = read_s16(0x1c,sf);
        xsb->simple_cues_count      = read_s16(0x1e,sf);
        /* 20(2): unknown count? (related to unknown2?) */
        xsb->wavebanks_count        = read_s16(0x22,sf);
        /* 24(10): xsb name */

        xsb->sounds_offset          = 0x34;
        xsb->wavebanks_name_size    = 0x10;
    }
    else if (xsb->version <= XSB_XACT1_2_MAX) {
        /* 06(2): crc */
        xsb->wavebanks_offset       = read_s32(0x08,sf);
        /* 0c(4): unknown1 offset (entry: 0x14) */
        /* 10(4): unknown2 offset (entry: variable) */
        /* 14(4): unknown3 offset */
        /* 18(2): empty? */
        /* 1a(2): element count? */
        xsb->complex_cues_count     = read_s16(0x1c,sf);
        xsb->simple_cues_count      = read_s16(0x1e,sf);
        /* 20(2): unknown count? (related to unknown2?) */
        xsb->wavebanks_count        = read_s16(0x22,sf);
        /* 24(4): null? */
        /* 28(10): xsb name */

        xsb->sounds_offset          = 0x38;
        xsb->wavebanks_name_size    = 0x10;
    }
    else if (xsb->version <= XSB_XACT2_MAX) {
        /* 06(2): crc */
        /* 08(1): platform? (3=X360) */
        xsb->simple_cues_count      = read_s16(0x09,sf);
        xsb->complex_cues_count     = read_s16(0x0B,sf);
        xsb->wavebanks_count        = read_s8 (0x11,sf);
        xsb->sounds_count           = read_s16(0x12,sf);
        /* 14(2): unknown */
        xsb->cue_names_size         = read_s32(0x16,sf);
        xsb->simple_cues_offset     = read_s32(0x1a,sf);
        xsb->complex_cues_offset    = read_s32(0x1e,sf);
        xsb->cue_names_offset       = read_s32(0x22,sf);
        /* 26(4): unknown */
        /* 2a(4): unknown */
        /* 2e(4): unknown */
        xsb->wavebanks_offset       = read_s32(0x32,sf);
        /* 36(4): cue name hash table offset? */
        xsb->nameoffsets_offset     = read_s32(0x3a,sf);
        xsb->sounds_offset          = read_s32(0x3e,sf);
        /* 42(4): unknown */
        /* 46(4): unknown */
        /* 4a(64): xsb name */

        xsb->wavebanks_name_size    = 0x40;
    }
    else {
        /* 06(2): format version */
        /* 08(2): crc (fcs16 checksum of all following data) */
        /* 0a(4): last modified low */
        /* 0e(4): last modified high */
        /* 12(1): platform? (1=PC, 3=X360) */
        xsb->simple_cues_count      = read_s16(0x13,sf);
        xsb->complex_cues_count     = read_s16(0x15,sf);
        /* 17(2): unknown count? */
        /* 19(2): element count? (often simple+complex cues, but may be more) */
        xsb->wavebanks_count        = read_s8 (0x1b,sf);
        xsb->sounds_count           = read_s16(0x1c,sf);
        xsb->cue_names_size         = read_s32(0x1e,sf);
        xsb->simple_cues_offset     = read_s32(0x22,sf);
        xsb->complex_cues_offset    = read_s32(0x26,sf);
        xsb->cue_names_offset       = read_s32(0x2a,sf);
        /* 0x2E(4): unknown offset */
        /* 0x32(4): variation tables offset */
        /* 0x36(4): unknown offset */
        xsb->wavebanks_offset       = read_s32(0x3a,sf);
        /* 0x3E(4): cue name hash table offset (16b each) */
        xsb->nameoffsets_offset     = read_s32(0x42,sf);
        xsb->sounds_offset          = read_s32(0x46,sf);
        /* 4a(64): xsb name */

        xsb->wavebanks_name_size    = 0x40;
    }

    //;VGM_LOG("XSB header: version=%i\n", xsb->version);
    //;VGM_LOG("XSB header: count: simple=%i, complex=%i, wavebanks=%i, sounds=%i\n",
    //        xsb->simple_cues_count, xsb->complex_cues_count, xsb->wavebanks_count, xsb->sounds_count);
    //;VGM_LOG("XSB header: offset: simple=%lx, complex=%lx, wavebanks=%lx, sounds=%lx\n",
    //        xsb->simple_cues_offset, xsb->complex_cues_offset, xsb->wavebanks_offset, xsb->sounds_offset);
    //;VGM_LOG("XSB header: names: cues=%lx, size=%x, hash=%lx\n",
    //        xsb->cue_names_offset, xsb->cue_names_size, xsb->nameoffsets_offset);

    if (xsb->version > XSB_XACT1_2_MAX && xsb->cue_names_size <= 0) {
        VGM_LOG("XSB: no names found\n");
        return 1;
    }


    /* find target wavebank */
    if (xsb->wavebanks_count) {
        char xsb_wavebank_name[64+1];
        int i;
        off_t offset;

        xsb->selected_wavebank = -1;

        offset = xsb->wavebanks_offset;
        for (i = 0; i < xsb->wavebanks_count; i++) {
            read_string(xsb_wavebank_name,xsb->wavebanks_name_size, offset,sf);
            //;VGM_LOG("XSB wavebanks: bank %i=%s\n", i, wavebank_name);
            if (strcasecmp(xsb_wavebank_name, xwb_wavebank_name)==0) {
                //;VGM_LOG("XSB banks: current xwb is wavebank %i=%s\n", i, xsb_wavebank_name);
                xsb->selected_wavebank = i;
            }

            offset += xsb->wavebanks_name_size;
        }

        //;VGM_LOG("xsb: selected wavebank=%i\n", xsb->selected_wavebank);
        if (xsb->selected_wavebank == -1) {
            VGM_LOG("XSB: current wavebank not found, selecting first\n");
            xsb->selected_wavebank = 0; //todo goto fail?
        }
    }


    /* find cue pointing to stream */
    if (xsb->version <= XSB_XACT1_2_MAX) {
        parse_xsb_cues_old(xsb, sf);
    }
    else {
        parse_xsb_cues_new(xsb, sf);
    }

    return 1;
fail:
    return 0;
}

static STREAMFILE * open_xsb_filename_pair(STREAMFILE *streamXwb) {
    STREAMFILE *streamXsb = NULL;
    /* .xwb to .xsb name conversion, since often they don't match */
    static const char *const filename_pairs[][2] = {
            {"MUSIC.xwb","Everything.xsb"},             /* Unreal Championship (Xbox) */
            {"Music.xwb","Sound Bank.xsb"},             /* Stardew Valley (Vita) */
            {"Ambiences_intro.xwb","Ambiences.xsb"},    /* Arx Fatalis (Xbox) */
            {"Wave*.xwb","Sound*.xsb"},                 /* XNA/MonoGame games? */
            {"*MusicBank.xwb","*SoundBank.xsb"},        /* NFL Fever 2004 (Xbox) */
            {"*_xwb","*_xsb"},                          /* Ikaruga (PC) */
            {"WB_*","SB_*"},                            /* Ikaruga (X360) */
            {"*StreamBank.xwb","*SoundBank.xsb"},       /* Eschatos (X360) */
            {"*WaveBank.xwb","*SoundBank.xsb"},         /* Eschatos (X360) */
            {"StreamBank_*.xwb","SoundBank_*.xsb"},     /* Ginga Force (X360) */
            {"WaveBank_*.xwb","SoundBank_*.xsb"},       /* Ginga Force (X360) */
            {"*_WB.xwb","*_SB.xsb"},                    /* Ninja Blade (X360) */
            {"*.xwb","*.xsb"},                          /* default */
    };
    int i;
    int pair_count = (sizeof(filename_pairs) / sizeof(filename_pairs[0]));
    char target_filename[PATH_LIMIT];
    char temp_filename[PATH_LIMIT];
    int target_len;

    /* try names in external .xsb, using a bunch of possible name pairs */
    get_streamfile_filename(streamXwb,target_filename,PATH_LIMIT);
    target_len = strlen(target_filename);

    for (i = 0; i < pair_count; i++) {
        const char *xwb_match = filename_pairs[i][0];
        const char *xsb_match = filename_pairs[i][1];
        size_t xwb_len = strlen(xwb_match);
        size_t xsb_len = strlen(xsb_match);
        int match_pos1 = -1, match_pos2 = -1, xwb_pos = -1 , xsb_pos = -1, new_len = 0;
        const char * teststr;


        //;VGM_LOG("XSB: pair1 '%s'='%s' << '%s' \n", xwb_match, xsb_match, target_filename);
        if (target_len < xwb_len)
            continue;

        /* ghetto string wildcard replace, ex:
         * - target filename = "start1_wildcard_end1", xwb_match = "start1_*_end1", xsb_match = "start2_*_end2"
         * > check xwb's "start_" starts in target_filename (from 0..xwb_pos), set match_pos1
         * > check xwb's "_end" ends in target_filename (from xwb_pos+1..end), set match_pos2
         * > copy xsb's "start2_" (from 0..xsb_pos)
         * > copy target "wildcard" (from 0..xsb_pos)
         * > copy xsb's "end" (from xsb_pos+1..end)
         * > final target_filename is "start2_wildcard_end2"
         * (skips start/end if wildcard is at start/end)
         */

        teststr = strchr(xwb_match, '*');
        if (teststr)
            xwb_pos = (intptr_t)teststr - (intptr_t)xwb_match;
        teststr = strchr(xsb_match, '*');
        if (teststr)
            xsb_pos = (intptr_t)teststr - (intptr_t)xsb_match;

        match_pos1 = 0;
        match_pos2 = target_len;
        temp_filename[0] = '\0';

        if (xwb_pos < 0) { /* no wildcard, check exact match */
            if (target_len != xwb_len || strncmp(target_filename, xwb_match, xwb_len))
                continue;
            strcpy(target_filename, xsb_match);
        }

        if (xwb_pos > 0) { /* wildcard after start, check starts_with  */
            int starts_len = xwb_pos;
            if (strncmp(target_filename + 0, xwb_match + 0, xwb_pos) != 0)
                continue;
            match_pos1 = 0 + starts_len;
        }

        if (xwb_pos >= 0 && xwb_pos + 1 < xwb_len) { /* wildcard before end, check ends_with  */
            int ends_len = xwb_len - (xwb_pos+1);
            if (strncmp(target_filename + target_len - ends_len, xwb_match + xwb_len - ends_len, ends_len) != 0)
                continue;
            match_pos2 = target_len - ends_len;
        }

        if (match_pos1 >= 0 && match_pos2 > match_pos1) { /* save match */
            int match_len = match_pos2 - match_pos1;
            strncpy(temp_filename, target_filename + match_pos1, match_len);
            temp_filename[match_len] = '\0';
        }

        if (xsb_pos > 0) { /* copy xsb start */
            strncpy(target_filename + 0, xsb_match, (xsb_pos));
            new_len += (xsb_pos);
            target_filename[new_len] = '\0';
        }

        if (xsb_pos >= 0){ /* copy xsb match */
            strncpy(target_filename + new_len, temp_filename, (match_pos2 - match_pos1));
            new_len += (match_pos2 - match_pos1);
            target_filename[new_len] = '\0';
        }

        if (xsb_pos >= 0 && xsb_pos + 1 < xsb_len) { /* copy xsb end */
            strncpy(target_filename + new_len, xsb_match + (xsb_pos+1), (xsb_len - (xsb_pos+1)));
            new_len += (xsb_len - (xsb_pos+1));
            target_filename[new_len] = '\0';
        }

        //;VGM_LOG("XSB: pair2 '%s'='%s' >> '%s'\n", xwb_match, xsb_match, target_filename);
        streamXsb = open_streamfile_by_filename(streamXwb, target_filename);
        if (streamXsb) return streamXsb;

        get_streamfile_filename(streamXwb,target_filename,PATH_LIMIT); /* reset for next loop */
    }

    return NULL;
}

#endif /* _XWB_XSB_H_ */
