#ifndef _FSB_KEYS_H_
#define _FSB_KEYS_H_
#include <stdint.h>

/*
 * List of known keys, found in aluigi's site (http://aluigi.altervista.org), forums, guessfsb.exe or manually.
 */

// Unknown:
// - Battle: Los Angeles
// - Guitar Hero: Warriors of Rock, DJ hero FSB
// - Longmenkezhan
// - Gas Guzzlers: Combat Carnage (PC?) "C5FA83EA64B34EC2BFE" hex or text? [FSB5]

typedef struct {
    uint8_t flags;
    const char* key;
    size_t key_size; /* precalc'd for speed */
} fsbkey_info;

#define FLAG_FSB4 (1 << 0)    /* key is valid for FSB4/3 */
#define FLAG_FSB5 (1 << 1)    /* key is valid for FSB5 */
#define FLAG_STD  (1 << 2)    /* regular XOR mode */
#define FLAG_ALT  (1 << 3)    /* alt XOR mode (seemingly not tied to FSB version or anything, maybe wrong key) */

#define MODE_FSB4_STD  (FLAG_FSB4 | FLAG_STD)
#define MODE_FSB4_ALT  (FLAG_FSB4 | FLAG_ALT)
#define MODE_FSB4_ALL  (FLAG_FSB4 | FLAG_ALT)
#define MODE_FSB5_STD  (FLAG_FSB5 | FLAG_STD)
#define MODE_FSB5_ALT  (FLAG_FSB5 | FLAG_STD)
#define MODE_FSB5_ALL  (FLAG_FSB5 | FLAG_STD | FLAG_ALT)
#define MODE_FSBS_STD  (FLAG_FSB4 | FLAG_FSB5 | FLAG_STD)
#define MODE_FSBS_ALL  (FLAG_FSB4 | FLAG_FSB5 | FLAG_STD | FLAG_ALT)

/* ugly macro for string + precomputed len (removing string's extra NULL)*/
#define FSBKEY_ADD(key) key, sizeof(key) - 1

static const fsbkey_info fsbkey_list[] = {
        { MODE_FSBS_STD, FSBKEY_ADD("DFm3t4lFTW") }, // Double Fine Productions: Brutal Legend, Massive Chalice, etc (multi)
        { MODE_FSB4_STD, FSBKEY_ADD("nos71RiT") }, // DJ Hero 2 (X360)
        { MODE_FSB5_STD, FSBKEY_ADD("H$#FJa%7gRZZOlxLiN50&g5Q") }, // N++ (PC?)
        { MODE_FSB5_STD, FSBKEY_ADD("sTOoeJXI2LjK8jBMOk8h5IDRNZl3jq3I") }, // Slightly Mad Studios: Project CARS (PC?), World of Speed (PC)
        { MODE_FSB5_STD, FSBKEY_ADD("%lAn2{Pi*Lhw3T}@7*!kV=?qS$@iNlJ") }, // Ghost in the Shell: First Assault (PC)
        { MODE_FSB5_STD, FSBKEY_ADD("1^7%82#&5$~/8sz") }, // RevHeadz Engine Sounds (Mobile)
        { MODE_FSB5_ALL, FSBKEY_ADD("FDPrVuT4fAFvdHJYAgyMzRF4EcBAnKg") }, // Dark Souls 3 (PC) [untested]
        { MODE_FSB4_ALL, FSBKEY_ADD("p&oACY^c4LK5C2v^x5nIO6kg5vNH$tlj") }, // Need for Speed Shift 2 Unleashed (PC demo?)[untested]
        { MODE_FSB5_STD, FSBKEY_ADD("996164B5FC0F402983F61F220BB51DC6") }, // Mortal Kombat X/XL (PC)
        { MODE_FSB5_STD, FSBKEY_ADD("logicsounddesignmwsdev") }, // Mirror War: Reincarnation of Holiness (PC)
        { MODE_FSBS_ALL, FSBKEY_ADD("gat@tcqs2010") }, // Xian Xia Chuan (PC) [untested]
        { MODE_FSBS_ALL, FSBKEY_ADD("j1$Mk0Libg3#apEr42mo") }, // Critter Crunch (PC), Superbrothers: Sword & Sworcery (PC) [untested]
        { MODE_FSBS_ALL, FSBKEY_ADD("@kdj43nKDN^k*kj3ndf02hd95nsl(NJG") }, // Cyphers [untested]
        { MODE_FSBS_ALL, FSBKEY_ADD("Xiayuwu69252.Sonicli81223#$*@*0") }, // Xuan Dou Zhi Wang / King of Combat [untested]
        { MODE_FSBS_ALL, FSBKEY_ADD("kri_tika_5050_") }, // Ji Feng Zhi Ren / Kritika Online [untested]
        { MODE_FSBS_ALL, FSBKEY_ADD("mint78run52") }, // Invisible Inc. (PC?) [untested]
        { MODE_FSBS_ALL, FSBKEY_ADD("5atu6w4zaw") }, // Guitar Hero 3 [untested]
        { MODE_FSBS_ALL, FSBKEY_ADD("B2A7BB00") }, // Supreme Commander 2 [untested]
        { MODE_FSB4_STD, FSBKEY_ADD("ghfxhslrghfxhslr") }, // Cookie Run: Ovenbreak
        { MODE_FSB4_ALT, FSBKEY_ADD("truck/impact/carbody") },// Monster Jam (PS2) [FSB3]
        { MODE_FSB4_ALT, FSBKEY_ADD("\xFC\xF9\xE4\xB3\xF5\x57\x5C\xA5\xAC\x13\xEC\x4A\x43\x19\x58\xEB\x4E\xF3\x84\x0B\x8B\x78\xFA\xFD\xBB\x18\x46\x7E\x31\xFB\xD0") },
        { MODE_FSB4_ALT, FSBKEY_ADD("\x8C\xFA\xF3\x14\xB1\x53\xDA\xAB\x2B\x82\x6B\xD5\x55\x16\xCF\x01\x90\x20\x28\x14\xB1\x53\xD8") },
        { MODE_FSB5_STD, FSBKEY_ADD("G0KTrWjS9syqF7vVD6RaVXlFD91gMgkC") }, // Sekiro: Shadows Die Twice (PC)
        { MODE_FSB5_STD, FSBKEY_ADD("BasicEncryptionKey") }, // SCP: Unity (PC) 
        { MODE_FSB5_STD, FSBKEY_ADD("FXnTffGJ9LS855Gc") }, // Worms Rumble Beta (PC)
        { MODE_FSB4_STD, FSBKEY_ADD("qjvkeoqkrdhkdckd") }, // Bubble Fighter (PC)
        { MODE_FSB5_STD, FSBKEY_ADD("p@4_ih*srN:UJk&8") }, // Fall Guys (PC) update ~2021-11
        { MODE_FSB5_STD, FSBKEY_ADD(",&.XZ8]fLu%caPF+") }, // Fall Guys (PC) update ~2022-07
        { MODE_FSB5_STD, FSBKEY_ADD("Achilles_0_15_DpG") }, // Achilles: Legends Untold (PC) 
        { MODE_FSB5_STD, FSBKEY_ADD("4FB8CC894515617939F4E1B7D50972D27213B8E6") }, // Cult of the Lamb Demo (PC) 
        { MODE_FSB5_STD, FSBKEY_ADD("X3EK%Bbga-%Y9HZZ%gkc*C512*$$DhRxWTGgjUG@=rUD") }, // Signalis (PC)
        { MODE_FSB5_STD, FSBKEY_ADD("281ad163160cfc16f9a22c6755a64fad") }, // Ash Echoes beta (Android)
        { MODE_FSB5_STD, FSBKEY_ADD("Aurogon666") }, // Afterimage demo (PC)
        { MODE_FSB5_STD, FSBKEY_ADD("IfYouLikeThosesSoundsWhyNotRenumerateTheir2Authors?") }, // Blanc (PC/Switch)
};
static const int fsbkey_list_count = sizeof(fsbkey_list) / sizeof(fsbkey_list[0]);


#endif /* _FSB_KEYS_H_ */
