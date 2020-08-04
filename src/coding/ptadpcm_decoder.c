#include "coding.h"


/* a somewhat IMA-like mix of pre-calculated [index][nibble][step,index] all in one */
static const int32_t ptadpcm_table[16][16][2] = {
    {
        {   -14,  2},  {   -10,  2},  {    -7,  1},  {    -5,  1},   {   -3,  0},   {   -2,  0},   {   -1,  0},   {    0,  0},
        {     0,  0},  {     1,  0},  {     2,  0},  {     3,  0},   {    5,  1},   {    7,  1},   {   10,  2},   {   14,  2},
    }, {
        {   -28,  3},  {   -20,  3},  {   -14,  2},  {   -10,  2},   {   -7,  1},   {   -5,  1},   {   -3,  1},   {   -1,  0},
        {     1,  0},  {     3,  1},  {     5,  1},  {     7,  1},   {   10,  2},   {   14,  2},   {   20,  3},   {   28,  3},
    }, {
        {   -56,  4},  {   -40,  4},  {   -28,  3},  {   -20,  3},   {  -14,  2},   {  -10,  2},   {   -6,  2},   {   -2,  1},
        {     2,  1},  {     6,  2},  {    10,  2},  {    14,  2},   {   20,  3},   {   28,  3},   {   40,  4},   {   56,  4},
    }, {
        {  -112,  5},  {   -80,  5},  {   -56,  4},  {   -40,  4},   {  -28,  3},   {  -20,  3},   {  -12,  3},   {   -4,  2},
        {     4,  2},  {    12,  3},  {    20,  3},  {    28,  3},   {   40,  4},   {   56,  4},   {   80,  5},   {  112,  5},
    }, {
        {  -224,  6},  {  -160,  6},  {  -112,  5},  {   -80,  5},   {  -56,  4},   {  -40,  4},   {  -24,  4},   {   -8,  3},
        {     8,  3},  {    24,  4},  {    40,  4},  {    56,  4},   {   80,  5},   {  112,  5},   {  160,  6},   {  224,  6},
    }, {
        {  -448,  7},  {  -320,  7},  {  -224,  6},  {  -160,  6},   { -112,  5},   {  -80,  5},   {  -48,  5},   {  -16,  4},
        {    16,  4},  {    48,  5},  {    80,  5},  {   112,  5},   {  160,  6},   {  224,  6},   {  320,  7},   {  448,  7},
    }, {
        {  -896,  8},  {  -640,  8},  {  -448,  7},  {  -320,  7},   { -224,  6},   { -160,  6},   {  -96,  6},   {  -32,  5},
        {    32,  5},  {    96,  6},  {   160,  6},  {   224,  6},   {  320,  7},   {  448,  7},   {  640,  8},   {  896,  8},
    }, {
        { -1792,  9},  { -1280,  9},  {  -896,  8},  {  -640,  8},   { -448,  7},   { -320,  7},   { -192,  7},   {  -64,  6},
        {    64,  6},  {   192,  7},  {   320,  7},  {   448,  7},   {  640,  8},   {  896,  8},   { 1280,  9},   { 1792,  9},
    }, {
        { -3584, 10},  { -2560, 10},  { -1792,  9},  { -1280,  9},   { -896,  8},   { -640,  8},   { -384,  8},   { -128,  7},
        {   128,  7},  {   384,  8},  {   640,  8},  {   896,  8},   { 1280,  9},   { 1792,  9},   { 2560, 10},   { 3584, 10},
    }, {
        { -7168, 11},  { -5120, 11},  { -3584, 10},  { -2560, 10},   {-1792,  9},   {-1280,  9},   { -768,  9},   { -256,  8},
        {   256,  8},  {   768,  9},  {  1280,  9},  {  1792,  9},   { 2560, 10},   { 3584, 10},   { 5120, 11},   { 7168, 11},
    }, {
        {-14336, 11},  {-10240, 11},  { -7168, 11},  { -5120, 11},   {-3584, 10},   {-2560, 10},   {-1536, 10},   { -512,  9},
        {   512,  9},  {  1536, 10},  {  2560, 10},  {  3584, 10},   { 5120, 11},   { 7168, 11},   {10240, 11},   {14336, 11},
    },  {
        {-28672, 11},  {-20480, 11},  {-14336, 11},  {-10240, 11},   {-7168, 11},   {-5120, 11},   {-3072, 11},   {-1024, 10},
        {  1024, 10},  {  3072, 11},  {  5120, 11},  {  7168, 11},   {10240, 11},   {14336, 11},   {20480, 11},   {28672, 11},
    },
    /* rest is 0s (uses up to index 12) */
};

/* Platinum "PtADPCM" custom ADPCM for Wwise (reverse engineered from .exes). */
void decode_ptadpcm(VGMSTREAMCHANNEL *stream, sample_t *outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, size_t frame_size) {
    uint8_t frame[0x104] = {0};
    off_t frame_offset;
    int i, frames_in, sample_count = 0, samples_done = 0;
    size_t bytes_per_frame, samples_per_frame;
    int16_t hist1, hist2;
    int index, nibble, step;

    /* external interleave (variable size), mono */
    bytes_per_frame = frame_size;
    samples_per_frame = 2 + (frame_size - 0x05) * 2;
    frames_in = first_sample / samples_per_frame;
    //first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame*frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */
    hist2 = get_s16le(frame + 0x00);
    hist1 = get_s16le(frame + 0x02);
    index = frame[0x04];

    VGM_ASSERT_ONCE(index > 12, "PTADPCM: incorrect index at %x\n", (uint32_t)frame_offset);
    if (index > 12)
        index = 12;

    /* write header samples (needed) */
    if (sample_count >= first_sample && samples_done < samples_to_do) {
        outbuf[samples_done * channelspacing] = hist2;
        samples_done++;
    }
    sample_count++;
    if (sample_count >= first_sample && samples_done < samples_to_do) {
        outbuf[samples_done * channelspacing] = hist1;
        samples_done++;
    }
    sample_count++;

    /* decode nibbles */
    for (i = 0; i < samples_per_frame - 2; i++) {
        uint8_t nibbles = frame[0x05 + i/2];
        int32_t sample;

        nibble = !(i&1) ? /* low nibble first */
                (nibbles >> 0) & 0xF :
                (nibbles >> 4) & 0xF;

        step  = ptadpcm_table[index][nibble][0];
        index = ptadpcm_table[index][nibble][1];
        sample = clamp16(step + 2*hist1 - hist2);

        if (sample_count >= first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = sample;
            samples_done++;
        }
        sample_count++;

        hist2 = hist1;
        hist1 = sample;
    }

    //stream->adpcm_history1_32 = hist1;
    //stream->adpcm_history2_32 = hist2;
}

size_t ptadpcm_bytes_to_samples(size_t bytes, int channels, size_t frame_size) {
    if (channels <= 0 || frame_size < 0x06) return 0;
    return (bytes / (channels * frame_size)) * (2 + (frame_size-0x05) * 2);
}
