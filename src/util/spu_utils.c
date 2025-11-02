#include <stdbool.h>
#include <math.h>
#include "spu_utils.h"

static int round10(int val) {
    int round_val = val % 10;
    if (round_val < 5) /* half-down rounding */
        return val - round_val;
    else
        return val + (10 - round_val);
}

int spu1_pitch_to_sample_rate_rounded(int pitch) {
    return round10((44100 * pitch) / 4096);
}

int spu2_pitch_to_sample_rate_rounded(int pitch) {
    return round10((48000 * pitch) / 4096);
}


int spu1_pitch_to_sample_rate(int pitch) {
    return ((44100.0 * pitch) / 4096.0);
}

int spu2_pitch_to_sample_rate(int pitch) {
    return ((48000 * pitch) / 4096);
}


/* Converts VAB note to PS1 pitch value (0-4096 where 4096 is 44100 Hz).
 * Function reversed from PS1 SDK. */
static uint16_t _svm_ptable[] = {
    4096, 4110, 4125, 4140, 4155, 4170, 4185, 4200,
    4216, 4231, 4246, 4261, 4277, 4292, 4308, 4323,
    4339, 4355, 4371, 4386, 4402, 4418, 4434, 4450,
    4466, 4482, 4499, 4515, 4531, 4548, 4564, 4581,
    4597, 4614, 4630, 4647, 4664, 4681, 4698, 4715,
    4732, 4749, 4766, 4783, 4801, 4818, 4835, 4853,
    4870, 4888, 4906, 4924, 4941, 4959, 4977, 4995,
    5013, 5031, 5050, 5068, 5086, 5105, 5123, 5142,
    5160, 5179, 5198, 5216, 5235, 5254, 5273, 5292,
    5311, 5331, 5350, 5369, 5389, 5408, 5428, 5447,
    5467, 5487, 5507, 5527, 5547, 5567, 5587, 5607,
    5627, 5648, 5668, 5688, 5709, 5730, 5750, 5771,
    5792, 5813, 5834, 5855, 5876, 5898, 5919, 5940,
    5962, 5983, 6005, 6027, 6049, 6070, 6092, 6114,
    6137, 6159, 6181, 6203, 6226, 6248, 6271, 6294,
    6316, 6339, 6362, 6385, 6408, 6431, 6455, 6478,
    6501, 6525, 6549, 6572, 6596, 6620, 6644, 6668,
    6692, 6716, 6741, 6765, 6789, 6814, 6839, 6863,
    6888, 6913, 6938, 6963, 6988, 7014, 7039, 7064,
    7090, 7116, 7141, 7167, 7193, 7219, 7245, 7271,
    7298, 7324, 7351, 7377, 7404, 7431, 7458, 7485,
    7512, 7539, 7566, 7593, 7621, 7648, 7676, 7704,
    7732, 7760, 7788, 7816, 7844, 7873, 7901, 7930,
    7958, 7987, 8016, 8045, 8074, 8103, 8133, 8162,
    8192
};

static uint16_t SsPitchFromNote(int16_t note, int16_t fine, uint8_t center, uint8_t fine_shift) {

    uint32_t pitch;
    int16_t calc, type;
    int32_t add, sfine;//, ret;

    sfine = fine + fine_shift;
    if (sfine < 0) sfine += 7;
    sfine >>= 3;

    add = 0;
    if (sfine > 15) {
        add = 1;
        sfine -= 16;
    }

    calc = add + (note - (center - 60));//((center + 60) - note) + add;
    pitch = _svm_ptable[16 * (calc % 12) + (int16_t)sfine];
    type = calc / 12 - 5;

    // regular shift
    if (type > 0) return pitch << type;
    // negative shift
    if (type < 0) return pitch >> -type;

    return pitch;
}

int spu1_note_to_pitch(int16_t note, int16_t fine, uint8_t center_note, uint8_t center_fine) {
    int pitch = SsPitchFromNote(note, fine, center_note, center_fine);
    if (pitch > 0x4000)
        pitch = 0x4000; // SPU clamp

    return pitch;
}


static const uint16_t note_pitch_table[12] = {
    0x8000, 0x879C, 0x8FAC, 0x9837, 0xA145, 0xAADC,
    0xB504, 0xBFC8, 0xCB2F, 0xD744, 0xE411, 0xF1A1
};

static const uint16_t fine_pitch_table[128] = {
    0x8000, 0x800E, 0x801D, 0x802C, 0x803B, 0x804A, 0x8058, 0x8067,
    0x8076, 0x8085, 0x8094, 0x80A3, 0x80B1, 0x80C0, 0x80CF, 0x80DE,
    0x80ED, 0x80FC, 0x810B, 0x811A, 0x8129, 0x8138, 0x8146, 0x8155,
    0x8164, 0x8173, 0x8182, 0x8191, 0x81A0, 0x81AF, 0x81BE, 0x81CD,
    0x81DC, 0x81EB, 0x81FA, 0x8209, 0x8218, 0x8227, 0x8236, 0x8245,
    0x8254, 0x8263, 0x8272, 0x8282, 0x8291, 0x82A0, 0x82AF, 0x82BE,
    0x82CD, 0x82DC, 0x82EB, 0x82FA, 0x830A, 0x8319, 0x8328, 0x8337,
    0x8346, 0x8355, 0x8364, 0x8374, 0x8383, 0x8392, 0x83A1, 0x83B0,
    0x83C0, 0x83CF, 0x83DE, 0x83ED, 0x83FD, 0x840C, 0x841B, 0x842A,
    0x843A, 0x8449, 0x8458, 0x8468, 0x8477, 0x8486, 0x8495, 0x84A5,
    0x84B4, 0x84C3, 0x84D3, 0x84E2, 0x84F1, 0x8501, 0x8510, 0x8520,
    0x852F, 0x853E, 0x854E, 0x855D, 0x856D, 0x857C, 0x858B, 0x859B,
    0x85AA, 0x85BA, 0x85C9, 0x85D9, 0x85E8, 0x85F8, 0x8607, 0x8617,
    0x8626, 0x8636, 0x8645, 0x8655, 0x8664, 0x8674, 0x8683, 0x8693,
    0x86A2, 0x86B2, 0x86C1, 0x86D1, 0x86E0, 0x86F0, 0x8700, 0x870F,
    0x871F, 0x872E, 0x873E, 0x874E, 0x875D, 0x876D, 0x877D, 0x878C
};

static uint16_t ps_note_to_pitch(uint16_t center_note, uint16_t center_fine, uint16_t note, int16_t fine) {
    /* Derived from OpenGOAL, Copyright (c) 2020-2022 OpenGOAL Team, ISC License
     *
     * Permission to use, copy, modify, and/or distribute this software for any
     * purpose with or without fee is hereby granted, provided that the above
     * copyright notice and this permission notice appear in all copies.
     *
     * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
     * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
     * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
     * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
     * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
     * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
     * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
     */

    int fine_adjust, fine_idx, note_adjust, note_idx;
    int unk1, unk2, unk3; /* TODO: better variable names */
    uint16_t pitch;

    fine_idx = fine + center_fine;

    fine_adjust = fine_idx;
    if (fine_idx < 0)
        fine_adjust = fine_idx + 0x7F;

    fine_adjust /= 128;
    note_adjust = note + fine_adjust - center_note;
    unk3 = note_adjust / 6;

    if (note_adjust < 0)
        unk3--;

    fine_idx -= fine_adjust * 128;

    if (note_adjust < 0)
        unk2 = -1;
    else
        unk2 = 0;
    if (unk3 < 0)
        unk3--;

    unk2 = (unk3 / 2) - unk2;
    unk1 = unk2 - 2;
    note_idx = note_adjust - (unk2 * 12);

    if ((note_idx < 0) || ((note_idx == 0) && (fine_idx < 0))) {
        note_idx += 12;
        unk1 = unk2 - 3;
    }

    if (fine_idx < 0) {
        note_idx = (note_idx - 1) + fine_adjust;
        fine_idx += (fine_adjust + 1) * 128;
    }

    pitch = (note_pitch_table[note_idx] * fine_pitch_table[fine_idx]) >> 16;

    if (unk1 < 0)
        pitch = (pitch + (1 << (-unk1 - 1))) >> -unk1;

    return pitch;
}

int spu2_note_to_pitch(int16_t note, int16_t fine, uint8_t center_note, uint8_t center_fine) {

    /* if it isn't, then it's treated as 44100 base? (PS1?) */
    bool is_negative = center_note >> 7; // center_note & 0x80;

    if (is_negative)
        center_note = 0x100 - center_note;

    int pitch = ps_note_to_pitch(center_note, center_fine, note, fine);
    if (pitch > 0x4000)
        pitch = 0x4000; // 192000 Hz max

    if (!is_negative) // PS1 mode?
        pitch = (pitch * 44100) / 48000;

    return pitch;
}


#if 0
static const uint32_t ssl_note_pitch_table[12] = {
    0x10000, 0x10F38, 0x11F59, 0x1306F, 0x1428A, 0x155B8,
    0x16A09, 0x17F91, 0x1965F, 0x1AE89, 0x1C823, 0x1E343,
};

static const uint32_t ssl_fine_pitch_table[256] = {
    0x10000, 0x1000E, 0x1001D, 0x1002C, 0x1003B, 0x10049, 0x10058, 0x10067,
    0x10076, 0x10085, 0x10094, 0x100A2, 0x100B1, 0x100C0, 0x100CF, 0x100DE,
    0x100ED, 0x100FB, 0x1010A, 0x10119, 0x10128, 0x10137, 0x10146, 0x10154,
    0x10163, 0x10172, 0x10181, 0x10190, 0x1019F, 0x101AE, 0x101BD, 0x101CC,
    0x101DA, 0x101E9, 0x101F8, 0x10207, 0x10216, 0x10225, 0x10234, 0x10243,
    0x10252, 0x10261, 0x10270, 0x1027E, 0x1028D, 0x1029C, 0x102AB, 0x102BA,
    0x102C9, 0x102D8, 0x102E7, 0x102F6, 0x10305, 0x10314, 0x10323, 0x10332,
    0x10341, 0x10350, 0x1035F, 0x1036E, 0x1037D, 0x1038C, 0x1039B, 0x103AA,
    0x103B9, 0x103C8, 0x103D7, 0x103E6, 0x103F5, 0x10404, 0x10413, 0x10422,
    0x10431, 0x10440, 0x1044F, 0x1045E, 0x1046D, 0x1047C, 0x1048B, 0x1049A,
    0x104A9, 0x104B8, 0x104C7, 0x104D6, 0x104E5, 0x104F5, 0x10504, 0x10513,
    0x10522, 0x10531, 0x10540, 0x1054F, 0x1055E, 0x1056D, 0x1057C, 0x1058B,
    0x1059B, 0x105AA, 0x105B9, 0x105C8, 0x105D7, 0x105E6, 0x105F5, 0x10604,
    0x10614, 0x10623, 0x10632, 0x10641, 0x10650, 0x1065F, 0x1066E, 0x1067E,
    0x1068D, 0x1069C, 0x106AB, 0x106BA, 0x106C9, 0x106D9, 0x106E8, 0x106F7,
    0x10706, 0x10715, 0x10725, 0x10734, 0x10743, 0x10752, 0x10761, 0x10771,
    0x10780, 0x1078F, 0x1079E, 0x107AE, 0x107BD, 0x107CC, 0x107DB, 0x107EA,
    0x107FA, 0x10809, 0x10818, 0x10827, 0x10837, 0x10846, 0x10855, 0x10865,
    0x10874, 0x10883, 0x10892, 0x108A2, 0x108B1, 0x108C0, 0x108D0, 0x108DF,
    0x108EE, 0x108FD, 0x1090D, 0x1091C, 0x1092B, 0x1093B, 0x1094A, 0x10959,
    0x10969, 0x10978, 0x10987, 0x10997, 0x109A6, 0x109B5, 0x109C5, 0x109D4,
    0x109E3, 0x109F3, 0x10A02, 0x10A12, 0x10A21, 0x10A30, 0x10A40, 0x10A4F,
    0x10A5E, 0x10A6E, 0x10A7D, 0x10A8D, 0x10A9C, 0x10AAB, 0x10ABB, 0x10ACA,
    0x10ADA, 0x10AE9, 0x10AF8, 0x10B08, 0x10B17, 0x10B27, 0x10B36, 0x10B46,
    0x10B55, 0x10B64, 0x10B74, 0x10B83, 0x10B93, 0x10BA2, 0x10BB2, 0x10BC1,
    0x10BD1, 0x10BE0, 0x10BF0, 0x10BFF, 0x10C0F, 0x10C1E, 0x10C2E, 0x10C3D,
    0x10C4D, 0x10C5C, 0x10C6C, 0x10C7B, 0x10C8B, 0x10C9A, 0x10CAA, 0x10CB9,
    0x10CC9, 0x10CD8, 0x10CE8, 0x10CF7, 0x10D07, 0x10D16, 0x10D26, 0x10D35,
    0x10D45, 0x10D55, 0x10D64, 0x10D74, 0x10D83, 0x10D93, 0x10DA2, 0x10DB2,
    0x10DC1, 0x10DD1, 0x10DE1, 0x10DF0, 0x10E00, 0x10E0F, 0x10E1F, 0x10E2F,
    0x10E3E, 0x10E4E, 0x10E5D, 0x10E6D, 0x10E7D, 0x10E8C, 0x10E9C, 0x10EAC,
    0x10EBB, 0x10ECB, 0x10EDB, 0x10EEA, 0x10EFA, 0x10F09, 0x10F19, 0x10F29,
};

// from LIBSSL.IRX (Square Sound Library?) in The Bouncer
static int square_note_to_pitch_ssl(uint16_t a1, uint16_t a2) {
    int idx = a1 + (a2 << 24);
    int unk0 = (idx >> 8) & 0xFF;
    int unk1 = (unk0 & 127);
    int unk2 = (128 << (unk1 / 12));

    int unk3 = *(int *) ((char *)ssl_fine_pitch_table + ((idx >> 14) & 1020));
    int note_idx = (unk1 - 12 * (unk1 / 12 - ((int)(unk1 << 16) >> 31)));

    int pitch = (((unk2 * ssl_note_pitch_table[note_idx]) >> 16) * unk3) >> 16;

    return pitch;
}

// from IOPSOUND.IRX in FF XI/XII
static int square_note_to_pitch_iop(int a1, int a2, int a3, int a4) {
    unsigned int v6; // $v1

    int fine_index = (a1 >> 12) + a2 + (a3 >> 0x10);
    int i;
    for (i = 0; fine_index < 0; --i)
        fine_index += 0xC00;
    int shift = (10 - (i + ((int)((uint64_t)(0x2AAAAAABLL * ((fine_index >> 8) & 0x7F)) >> 0x20) >> 1)));
    v6 = ((ssl_note_pitch_table[((fine_index >> 8) & 0x7F) - 12
        * ((int)((uint64_t)(0x2AAAAAABLL * ((fine_index >> 8) & 0x7F)) >> 32) >> 1)] >> shift)
        * ssl_fine_pitch_table[(uint8_t)fine_index]) >> 0x10;

    if ( a4 ) {
        if ( a4 <= 0 )
            return (int)(v6 * (unsigned __int8)a4) >> 8;
        else
            v6 += (int)(v6 * (a4 + 1)) >> 7;
    }

    return v6;
}
#endif

// TODO: fix with the above formulas
// info from: https://github.com/BlackFurniture/ffcc/blob/master/ffcc/audio.py
int square_key_to_sample_rate(int32_t key, int base_rate) {
    int sample_rate = (int)round(base_rate * pow(2.0, key / (double)0x1000000 / 12.0));
    // some FF XI .wd
    if (sample_rate >= base_rate)
        sample_rate = base_rate;
    return sample_rate;
}
