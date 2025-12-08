#ifndef _FSB_KEYS_H_
#define _FSB_KEYS_H_
#include <stdint.h>

/* List of known keys, some found in aluigi's site (http://aluigi.altervista.org), forums, guessfsb.exe or manually.
 */

typedef struct {
    uint8_t flags;
    const char* key;
    size_t key_size;
} fsbkey_info;

#define FLAG_FSB4   (1 << 0)    // key is valid for FSB3/4
#define FLAG_FSB5   (1 << 1)    // key is valid for FSB5
#define FLAG_STD    (1 << 2)    // regular XOR mode
#define FLAG_ALT    (1 << 3)    // alt XOR mode (seemingly older files or possibly FSB3 only)

#define MODE_FSB3   (FLAG_FSB4 | FLAG_ALT)
#define MODE_FSB4   (FLAG_FSB4 | FLAG_STD)
#define MODE_FSB5   (FLAG_FSB5 | FLAG_STD)
#define MODE_FSBS   (FLAG_FSB4 | FLAG_FSB5 | FLAG_STD | FLAG_ALT)

/* ugly macro for string + precomputed len (removing string's extra NULL)*/
#define FSBKEY_ADD(key) key, sizeof(key) - 1

static const fsbkey_info fsbkey_list[] = {
        { MODE_FSB4, FSBKEY_ADD("DFm3t4lFTW") }, // Double Fine Productions: Brutal Legend, Massive Chalice, etc (multi)
        { MODE_FSB5, FSBKEY_ADD("DFm3t4lFTW") }, // Double Fine Productions
        { MODE_FSB4, FSBKEY_ADD("nos71RiT") }, // DJ Hero 2 (X360)
        { MODE_FSB5, FSBKEY_ADD("H$#FJa%7gRZZOlxLiN50&g5Q") }, // N++ (PC?)
        { MODE_FSB5, FSBKEY_ADD("sTOoeJXI2LjK8jBMOk8h5IDRNZl3jq3I") }, // Slightly Mad Studios: Project CARS (PC?), World of Speed (PC)
        { MODE_FSB5, FSBKEY_ADD("%lAn2{Pi*Lhw3T}@7*!kV=?qS$@iNlJ") }, // Ghost in the Shell: First Assault (PC)
        { MODE_FSB5, FSBKEY_ADD("1^7%82#&5$~/8sz") }, // RevHeadz Engine Sounds (Mobile)
        { MODE_FSB5, FSBKEY_ADD("FDPrVuT4fAFvdHJYAgyMzRF4EcBAnKg") }, // Dark Souls 3 (PC)
        { MODE_FSB4, FSBKEY_ADD("p&oACY^c4LK5C2v^x5nIO6kg5vNH$tlj") }, // Need for Speed Shift 2 Unleashed (PC)
        { MODE_FSB5, FSBKEY_ADD("996164B5FC0F402983F61F220BB51DC6") }, // Mortal Kombat X/XL (PC)
        { MODE_FSB5, FSBKEY_ADD("logicsounddesignmwsdev") }, // Mirror War: Reincarnation of Holiness (PC)
        { MODE_FSBS, FSBKEY_ADD("gat@tcqs2010") }, // Xian Xia Chuan (PC) [untested]
        { MODE_FSBS, FSBKEY_ADD("j1$Mk0Libg3#apEr42mo") }, // Critter Crunch (PC), Superbrothers: Sword & Sworcery (PC) [untested]
        { MODE_FSBS, FSBKEY_ADD("@kdj43nKDN^k*kj3ndf02hd95nsl(NJG") }, // Cyphers [untested]
        { MODE_FSBS, FSBKEY_ADD("Xiayuwu69252.Sonicli81223#$*@*0") }, // Xuan Dou Zhi Wang / King of Combat [untested]
        { MODE_FSBS, FSBKEY_ADD("kri_tika_5050_") }, // Ji Feng Zhi Ren / Kritika Online [untested]
        { MODE_FSBS, FSBKEY_ADD("mint78run52") }, // Invisible Inc. (PC?) [untested]
        { MODE_FSBS, FSBKEY_ADD("5atu6w4zaw") }, // Guitar Hero 3 [untested]
        { MODE_FSB4, FSBKEY_ADD("B2A7BB00") }, // Supreme Commander 2 (PC)
        { MODE_FSB4, FSBKEY_ADD("ghfxhslrghfxhslr") }, // Cookie Run: Ovenbreak
        { MODE_FSB3, FSBKEY_ADD("truck/impact/carbody") }, // Monster Jam (PS2) [FSB3]
        { MODE_FSB5, FSBKEY_ADD("G0KTrWjS9syqF7vVD6RaVXlFD91gMgkC") }, // Sekiro: Shadows Die Twice (PC)
        { MODE_FSB5, FSBKEY_ADD("BasicEncryptionKey") }, // SCP: Unity (PC) 
        { MODE_FSB5, FSBKEY_ADD("FXnTffGJ9LS855Gc") }, // Worms Rumble Beta (PC)
        { MODE_FSB4, FSBKEY_ADD("qjvkeoqkrdhkdckd") }, // Bubble Fighter (PC)
        { MODE_FSB5, FSBKEY_ADD("p@4_ih*srN:UJk&8") }, // Fall Guys (PC) update ~2021-11
        { MODE_FSB5, FSBKEY_ADD(",&.XZ8]fLu%caPF+") }, // Fall Guys (PC) update ~2022-07
        { MODE_FSB5, FSBKEY_ADD("^*4[hE>K]x90Vj") }, // Fall Guys (PC) update ~2023-05
        { MODE_FSB5, FSBKEY_ADD("Achilles_0_15_DpG") }, // Achilles: Legends Untold (PC) 
        { MODE_FSB5, FSBKEY_ADD("4FB8CC894515617939F4E1B7D50972D27213B8E6") }, // Cult of the Lamb Demo (PC) 
        { MODE_FSB5, FSBKEY_ADD("X3EK%Bbga-%Y9HZZ%gkc*C512*$$DhRxWTGgjUG@=rUD") }, // Signalis (PC)
        { MODE_FSB5, FSBKEY_ADD("281ad163160cfc16f9a22c6755a64fad") }, // Ash Echoes beta (Android)
        { MODE_FSB5, FSBKEY_ADD("Aurogon666") }, // Afterimage demo (PC)
        { MODE_FSB5, FSBKEY_ADD("IfYouLikeThosesSoundsWhyNotRenumerateTheir2Authors?") }, // Blanc (PC/Switch)
        { MODE_FSB5, FSBKEY_ADD("L36nshM520") }, // Nishuihan Mobile (Android)
        { MODE_FSB5, FSBKEY_ADD("Forza2!") }, // Forza Motorsport (PC)
        { MODE_FSB5, FSBKEY_ADD("cbfjZTlUPaZI") }, // JDM: Japanese Drift Master (PC)
        { MODE_FSB3, FSBKEY_ADD("tkdnsem000") }, // Ys Online: The Call of Solum (PC) [FSB3] (alt key: 2ED62676CEA6B60C0C0C)
        { MODE_FSB4, FSBKEY_ADD("4DxgpNV3pQLPD6GT7g9Gf6eWU7SXutGQ") }, // Test Drive: Ferrari Racing Legends (PC)
        { MODE_FSB5, FSBKEY_ADD("AjaxIsTheGoodestBoy") }, // Hello Kitty: Island Adventure (iOS)
        { MODE_FSB5, FSBKEY_ADD("resoforce") }, // Rivals of Aether 2 (PC)
        { MODE_FSB5, FSBKEY_ADD("3cfe772db5b55b806541d3faf894020e") }, // Final Fantasy XV: War for Eos (Android)
        { MODE_FSB5, FSBKEY_ADD("aj#$kLucf2lh}eqh") }, // Forza Motorsport 2023 (PC)
        { MODE_FSB4, FSBKEY_ADD("dpdjeoqkr") }, // AirRider CrazyRacing (PC)
        { MODE_FSB5, FSBKEY_ADD("weareAbsolutelyUnsure2018") }, // Wanderstop (PC)
        { MODE_FSB5, FSBKEY_ADD(".xW3uXQ8q79yunvMjL6nahLXts9esEXX2VgetuPCxdLrAjUUbZAmB7R*A6KjW24NU_8ifMZ8TC4Qk@_oEsjsK2QLpAaG-Fy!wYKP") }, // UNBEATABLE Demo (PC)
        { MODE_FSB5, FSBKEY_ADD(",H9}:p?`bRlQG5_yJ\"\"/L,X_{:=Gs1") }, // Rennsport (PC)
        { MODE_FSB5, FSBKEY_ADD("K50j8B2H4pVUfzt7yxfTprg9wdr9zIH6") }, // Gunner, HEAT, PC! (PC)
        { MODE_FSB5, FSBKEY_ADD("Panshen666") }, // Duet Night Abyss (PC)-beta
        { MODE_FSB5, FSBKEY_ADD("M2QEEj6au7Nx0pgYpl8Uhqe9R3CWEjPGbPK6KENwG9eypOkpYq") }, // Undisputed (PC) 
        { MODE_FSB5, FSBKEY_ADD("+1@+#{n`<h0(r|:1") }, // Wreckreation (PC)
        { MODE_FSB5, FSBKEY_ADD("MyPjgFmodKey2020^.^") }, // The Tale of Food / Shi Wu Yu (Android)
        { MODE_FSB5, FSBKEY_ADD("gfnalknasfg02930293fdksj098234fjeijfiejfei030") }, // ScourgeBringer (Switch)
        { MODE_FSB5, FSBKEY_ADD("skkpycwtxzxnbozd0hb1ial0hxnrbuo0") }, // Return to Monkey Island (PC/)

        /* some games use a key per file, generated from the filename
         * (could add all of them but there are a lot of songs, so external .fsbkey are probably better) */
        //{ MODE_FSB4_STD, FSBKEY_ADD("...") }, // Guitar Hero: Metallica (PC/PS3/X360) [FSB4]
        //{ MODE_FSB4_STD, FSBKEY_ADD("...") }, // Guitar Hero: World Tour (PC/PS3/X360) [FSB4]
        //{ MODE_FSB4_STD, FSBKEY_ADD("...") }, // Guitar Hero 5 (PC/PS3/X360) [FSB4] (streams seem to use the same default key)
};
static const int fsbkey_list_count = sizeof(fsbkey_list) / sizeof(fsbkey_list[0]);

#endif
