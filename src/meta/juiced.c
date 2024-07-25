#include "meta.h"

#define MAX_SOUNDS_PER_DAT 1000

typedef enum { PCM, WMA, XIMA, VAG, RIFF_AT3P, MSF_AT3 } juiced_codec;

int parse_juiced_dat_header(STREAMFILE* sf);

/* Juice Games' bespoke format for their internal sound system
 * [Juiced (PS2/Xbox/PC), Juiced: Eliminator (PSP), Juiced 2: Hot Import Nights (PS2/PSP/PS3/X360(?)/PC)] */
VGMSTREAM* init_vgmstream_juiced_dat(STREAMFILE* sf)
{
    VGMSTREAM* vgmstream = NULL;

    /* checks, starting with extension, then the first 32-bit field  from the file. */
    if (!check_extensions(sf,"dat,ldat")) return NULL;
    /* (todo) there's "dsb" (PC ver) and "sdt" (PS2/PSP/PS3) */
    if (!parse_juiced_dat_header(sf)) return NULL;

    /* Juice Games' crafted two "dat" formats for use in their own games:
     * one for general file storage (aka bigfiles), other for in-game sounds.
     * vgmstream code will be focusing on the latter, full stop. */
}

int parse_juiced_dat_header(STREAMFILE* sf)
{
    off_t sound_info_offset, sound_data_base_offset, sound_data_real_offset;
    int32_t num_sounds;

    int32_t (*read_s32)(off_t,STREAMFILE*) = NULL;
    uint32_t (*read_u32)(off_t,STREAMFILE*) = NULL;

    if ((read_u32be(0,sf) >= 0x00020000) && (read_u32be(0,sf) <= 0x01000000)) { read_s32 = read_s32le; read_u32 = read_u32le; }
    else if ((read_u32be(0,sf) >= 0x00000001) && (read_u32be(0,sf) <= 0x00000200)) { read_s32 = read_s32be; read_u32 = read_u32be; }
    else { return -1; }

    num_sounds = read_s32(0,sf);

    if (num_sounds != 0) { if (num_sounds >= MAX_SOUNDS_PER_DAT) { return -1; } }

    sound_info_offset = 4;
    sound_data_base_offset = num_sounds * 0xa4;

    /* format is as follows:
     * 0x00 - could be either sound data size (for PCM and XIMA sounds) or some float/int value
     * 0x04 - codec type? codec number?
              0x12 - can be PCM or WMAv1 (0x161 codec ID as per WAVEFORMATEX spec)
                     do note that some sounds are identified as PCM but dsb has them as one big wma
              0x14 - XIMA (Xbox IMA ADPCM)
     * 0x08-0x90 - codec metadata
     * 0x90 - offset to sound data
     * 0x94-0xa4 - sound name */

    /* (todo) Microsoft ASF patents are the following: 6041345, 6327652, 6330670 and 6389473. they have all expired.
     * WMAv1 has a lot of patents filed by Microsft as well, research those or just chat them to Bing Copilot. */
    /* (todo) how Juice Games approached streamed sound data is as follows:
     *
     * for PC games, they're stored in one bigfile, set to "dsb" extension.
     * said bigfile is a wma containing ALL the sounds, and if played on its own they ALL play immediately after each other.
     * to get to some selected sound, wma is seeked to some "frame" (in int format, for music) or "duration" (in float format, for voice lines)
     * then it gets feeded a separate number containing either "frames" or "duration" for that particular sound.
     * what happens next is some sound inside wma is actually played in-game with two set "start offset" and "duration" values of their own.
     * and that should be it, right?
     *
     * for PS2/PSP/PS3 games, they're stored in one bigfile, set to "sdt" extension.
     * said bigfile consists of either one or many VAGp, RIFF or MSF\x02 files, and their offsets are sourced elsewhere.
     *
     * for Xbox/Xbox360 games, Juice Games employed a XACT(xsb+xwb)+WMA combo.
     * the former is used for voice lines, the latter is used for music and (assuming 16 music tracks in the game) is stored into 2-3MB files each. */
}
