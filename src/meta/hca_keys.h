#ifndef _HCA_KEYS_H_
#define _HCA_KEYS_H_

typedef struct {
    uint64_t key;
} hcakey_info;

/* CRI's tools expect an unsigned 64 bit number, but keys are commonly found online in hex form */
static const hcakey_info hcakey_list[] = {

        // HCA Decoder default
        {9621963164387704},         // CF222F1FE0748978

        // Phantasy Star Online 2 (multi?)
        // used by most console games
        {0xCC55463930DBE1AB},       // CC55463930DBE1AB / 14723751768204501419
        // variation from VGAudio, but some 2ch poster says the above works with CRI's tools; seems to decode the same
        {24002584467202475},        // 0055463930DBE1AB

        // Old Phantasy Star Online 2 (multi?)
        {61891147883431481},        // 30DBE1ABCC554639

        // Jojo All Star Battle (PS3)
        {19700307},                 // 00000000012C9A53

        // Ro-Kyu-Bu! Himitsu no Otoshimono (PSP)
        {2012082716},               // 0000000077EDF21C

        // Ro-Kyu-Bu! Naisho no Shutter Chance (PSV)
        {1234253142},               // 0000000049913556

        // Idolm@ster Cinderella Stage (iOS/Android)
        // Shadowverse (iOS/Android)
        {59751358413602},           // 00003657F27E3B22

        // Grimoire ~Shiritsu Grimoire Mahou Gakuen~ (iOS/Android)
        {5027916581011272},         // 0011DCDD0DC57F48

        // Idol Connect (iOS/Android)
        {2424},                     // 0000000000000978

        // Kamen Rider Battle Rush (iOS/Android)
        {29423500797988784},        // 00688884A11CCFB0

        // SD Gundam Strikers (iOS/Android)
        {30260840980773},           // 00001B85A6AD6125

        // Sonic Runners (iOS/Android)
        {19910623},                 // 00000000012FCFDF

        // Fate/Grand Order (iOS/Android) base assets
        {12345},                    // 0000000000003039

        // Fate/Grand Order (iOS/Android) download assets *unconfirmed
        {9117927877783581796},      // 7E89631892EBF464

        // Raramagi (iOS/Android)
        {45719322},                 // 0000000002B99F1A

        // Idolm@ster Million Live (iOS/Android)
        {765765765765765},          // 0002B875BC731A85

        // Kurokishi to Shiro no Maou (iOS/Android)
        {3003875739822025258},      // 29AFE911F5816A2A

        // Puella Magi Madoka Magica Side Story: Magia Record (iOS/Android)
        // Hortensia Saga (iOS/Android)
        {20536401},                 // 0000000001395C51

        // The Tower of Princess (iOS/Android)
        {9101518402445063},         // 002055C8634B5F07

        // Fallen Princess (iOS/Android)
        {145552191146490718},       // 02051AF25990FB5E

        // Diss World (iOS/Android)
        {9001712656335836006},      // 7CEC81F7C3091366

        // Ikemen Vampire - Ijin-tachi to Koi no Yuuwaku (iOS/Android)
        {45152594117267709},        // 00A06A0B8D0C10FD

        // Super Robot Wars X-Omega (iOS/Android)
        {165521992944278},          // 0000968A97978A96

        // BanG Dream! Girls Band Party! (iOS/Android)
        {8910},                     // 00000000000022CE

        // Tokyo 7th Sisters (iOS/Android) *unconfirmed
        {0xFDAE531AAB414BA1},       // FDAE531AAB414BA1

        // One Piece Dance Battle (iOS/Android)
        {1905818},                  // 00000000001D149A

        // Derby Stallion Masters (iOS/Android)
        {19840202},                 // 00000000012EBCCA

        // World Chain (iOS/Android)
        {4892292804961027794},      // 43E4EA62B8E6C6D2

        // Yuyuyui (iOS/Android) *unconfirmed
        {4867249871962584729},      // 438BF1F883653699

};


#endif/*_HCA_KEYS_H_*/
