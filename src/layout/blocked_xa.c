#include "layout.h"
#include "../coding/coding.h"
#include "../vgmstream.h"

/* parse a CD-XA raw mode2/form2 sector */
void block_update_xa(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;
    int i, is_audio;
    size_t block_samples;
    uint16_t xa_config, target_config;
    uint8_t xa_submode, xa_header;


    /* XA mode2/form2 sector, size 0x930
     * 0x00: sync word
     * 0x0c: header = minute, second, sector, mode (always 0x02)
     * 0x10: subheader = file, channel, submode flags, xa header
     * 0x14: subheader again (for error correction)
     * 0x18: data
     * 0x918: unused
     * 0x92c: EDC/checksum or null
     * 0x930: end
     * Sectors with no data may exist near other with data
     */
    xa_config = read_u16be(block_offset + 0x10, sf); /* file + channel */

    /* submode flag bits (typical audio value = 0x64 01100100)
     * - 7 (0x80 10000000): end of file (usually at last sector of a channel or at data end)
     * - 6 (0x40 01000000): real time sector (special control flag)
     * - 5 (0x20 00100000): sector form (0=form1, 1=form2)
     * - 4 (0x10 00010000): trigger (generates interrupt for the application)
     * - 3 (0x08 00001000): data sector
     * - 2 (0x04 00000100): audio sector
     * - 1 (0x02 00000010): video sector
     * - 0 (0x01 00000001): end of audio (optional for non-real time XAs)
     * Empty sectors with no flags may exist interleaved with other with audio/data.
     */
    xa_submode = read_u8(block_offset + 0x12, sf);

    /* header bits:
     * - 7 (0x80 10000000): reserved
     * - 6 (0x40 01000000): emphasis (applies filter, same as CD-DA emphasis)
     * - 4 (0x30 00110000): bits per sample (0=4-bit, 1=8-bit)
     * - 2 (0x0C 00001100): sample rate (0=37.8hz, 1=18.9hz)
     * - 0 (0x03 00000011): channels (0=mono, 1=stereo)
     */
    xa_header = read_u8(block_offset + 0x13, sf);

    /* Sector subheader's file+channel markers are used to interleave streams (music/sfx/voices)
     * by reading one target file+channel while ignoring the rest. This is needed to adjust
     * CD drive spinning <> decoding speed (data is read faster otherwise, so can't have 2
     * sectors of the same channel).
     *
     * Normally N channels = N streams (usually 8/16/32 depending on sample rate/stereo or even higher),
     * though channels can be empty or contain video (like 7 or 15 video sectors + 1 audio frame).
     * Usually nterleaved channels use with the same file ID, but some games change ID too. Channels
     * don't always follow an order (so 00,01,02,08,06,00 is possible though uncommon, ex. Spyro 2)
     *
     * Extractors deinterleave and split .xa using file + channel + EOF flags.
     * 'Channel' here doesn't mean "audio channel", just a fancy name for substreams (mono or stereo).
     * Files can go up to 255, normally file 0=sequential, 1+=interleaved */
    target_config = vgmstream->codec_config;

    /* audio sector must set/not set certain flags, as per spec (in theory form2 only too) */
    is_audio = !(xa_submode & 0x08) && (xa_submode & 0x04) && !(xa_submode & 0x02);

    if (xa_config != target_config) {
        block_samples = 0; /* not a target sector */
    }
    else if (is_audio) {
        int subframes = ((xa_header >> 4) & 3) == 1 ? 4 : 8; /* 8-bit mode = 4 subframes */
        if (xa_submode & 0x20) {
            /* form2 audio: size 0x900, 18 frames of size 0x80 with N subframes of 28 samples */
            block_samples = (28*subframes / vgmstream->channels) * 18;
        }
        else { /* rare, found with empty audio [Glint Glitters (PS1), Dance! Dance! Dance! (PS1)] */
            /* form1 audio: size 0x800, 16 frames of size 0x80 with N subframes of 28 samples (rest is garbage/other data) */
            block_samples = (28*subframes / vgmstream->channels) * 16;
        }
    }
    else {
        block_samples = 0; /* not an audio sector */
    }

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_samples = block_samples;
    vgmstream->next_block_offset = block_offset + 0x930;

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + 0x18;
    }
}
