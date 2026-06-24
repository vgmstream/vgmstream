#include "layout.h"
#include "../coding/coding.h"
#include "../vgmstream.h"

/* parse a CD-XA raw mode2 sector */
void block_update_xa(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;


    /* XA mode2 sector, size 0x930
     * 0x00: sync word
     * 0x0c: sector header = minute, second, sector number, mode
     * 0x10: 'xa subheader' = file, channel, submode flags, coding info
     * 0x14: 'xa subheader' again (for CD error correction)
     * 0x18: data (size: form1=0x800 + 0x100, form2=0x900)
     * 0x918: unused
     * 0x92c: EDC/checksum or null
     * 0x930: end
     * Sectors with no data may exist near other with data
     */
    uint32_t sector_sync    = read_u32be(block_offset + 0x00, sf);
    uint32_t xa_subheader   = read_u32be(block_offset + 0x10, sf);

    // detect EOF
    if (sector_sync != 0x00FFFFFF || xa_subheader == 0xFFFFFFFF) {
        vgmstream->current_block_samples = -1;
        return;
    }

    /* xa submode bits (typical audio value = 0x64 01100100)
     * - 7 (0x80 10000000): end of 'file' (usually at last sector of a channel or at data end)
     * - 6 (0x40 01000000): real time sector (special control flag)
     * - 5 (0x20 00100000): sector form (0=form1, 1=form2)
     * - 4 (0x10 00010000): trigger (generates interrupt for the application)
     * - 3 (0x08 00001000): data sector
     * - 2 (0x04 00000100): audio sector
     * - 1 (0x02 00000010): video sector
     * - 0 (0x01 00000001): end of audio (optional for non-real time XAs)
     * Empty sectors with no flags may exist interleaved with other with audio/data.
     */
    uint8_t xa_submode = (xa_subheader >> 8) & 0xFF;

    /* xa header bits:
     * - 7 (0x80 10000000): reserved
     * - 6 (0x40 01000000): emphasis (applies filter, same as CD-DA emphasis)
     * - 4 (0x30 00110000): bits per sample (0=4-bit, 1=8-bit)
     * - 2 (0x0C 00001100): sample rate (0=37.8hz, 1=18.9hz)
     * - 0 (0x03 00000011): channels (0=mono, 1=stereo)
     */
    uint8_t xa_header = (xa_subheader >> 0) & 0xFF;

    /* In CD-XA, multiple streams must be interleaved since the CD drive reads sectors faster than MDEC/XA decodes
     * them. The subheader's file + channel markers are used to filter and play target sectors (streams).
     *
     * Normally N channels = N streams. Total are 4/8/16/32 depending on CD x1/x2 speed + sample rate + stereo.
     * Channels can be empty for padding purposes, or contain video (7 or 15 video sectors + 1 audio frame).
     * Typically interleaved channels use with the same file ID, but some games change ID too. Channels
     * don't always follow an order (so 00,01,02,08,06,00 is possible though uncommon, ex. Spyro 2).
     *
     * Extractors deinterleave and split .xa using file + channel + EOF flags, and paste sectors together,
     * but raw XA data (AFAIK) can't handle 2 sequential sectors of the same file + channel.
     * 'Channel' here doesn't mean "audio channel", just a fancy name for substreams.
     * Files can go up to 255, normally file 0=sequential, 1+=interleaved */
    uint16_t xa_config = (xa_subheader >> 16) & 0xFFFF; // file + channel
    uint16_t target_config = vgmstream->codec_config;


    /* audio sector must set/not set certain flags, as per spec (in theory form2 only too) */
    bool is_audio = !(xa_submode & 0x08) && (xa_submode & 0x04) && !(xa_submode & 0x02);

    int block_samples;
    if (!is_audio || xa_config != target_config) {
        block_samples = 0; // not a audio or target sector
    }
    else {
        bool is_8bit = ((xa_header >> 4) & 3) == 1;
        int subframes = is_8bit ? 4 : 8;
        if (xa_submode & 0x20) {
            // form2 audio: size 0x900, 18 frames of size 0x80 with N subframes of 28 samples
            block_samples = (28 * subframes / vgmstream->channels) * 18;
        }
        else {
            /* rare, found with empty audio [Glint Glitters (PS1), Dance! Dance! Dance! (PS1)] */
            /* form1 audio: size 0x800, 16 frames of size 0x80 with N subframes of 28 samples (rest is garbage/other data) */
            block_samples = (28 * subframes / vgmstream->channels) * 16;
        }
    }

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_samples = block_samples;
    vgmstream->next_block_offset = block_offset + 0x930;

    for (int i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + 0x18;
    }
}
