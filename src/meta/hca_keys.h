#ifndef _HCA_KEYS_H_
#define _HCA_KEYS_H_

#include "hca_keys_awb.h"

typedef struct {
    uint64_t key;               /* hca key or seed ('user') key */
    const uint16_t *subkeys;    /* scramble subkey table for seed key */
    size_t subkeys_size;        /* size of the derivation subkey table */
} hcakey_info;


/**
 * List of known keys, extracted from the game files (mostly found in 2ch.net).
 * CRI's tools expect an unsigned 64 bit number string, but keys are commonly found online in hex form.
 * Keys only use 56 bits though, so the upper 8 bits can be ignored.
 *
 * Some ACB+AWB after mid 2018 use a user seed key + a scramble subkey in the AWB (normally 16b LE at 0x0e)
 * to create the final HCA key, which means there is one key per AWB (so most HCA have a unique key).
 * vgmstream derives the key if subkey table is provided.
 */
static const hcakey_info hcakey_list[] = {

        // CRI HCA decoder default
        {9621963164387704},         // CF222F1FE0748978

        // Phantasy Star Online 2 (multi?)
        // used by most console games
        {14723751768204501419u},    // CC55463930DBE1AB

        // Old Phantasy Star Online 2 (multi?)
        {61891147883431481},        // 30DBE1ABCC554639

        // Jojo All Star Battle (PS3)
        {19700307},                 // 00000000012C9A53

        // Ro-Kyu-Bu! Himitsu no Otoshimono (PSP)
        {2012082716},               // 0000000077EDF21C

        // VRIDGE Inc. games:
        // - HatsuKare * Renai Debut Sengen! (PSP)
        // - Seitokai no Ichizon Lv. 2 Portable (PSP)
        // - Koi wa Kousoku ni Shibararenai! (PSP)
        // - StormLover 2nd (PSP)
        // - Prince of Stride (PSVita)
        // - Ro-Kyu-Bu! Naisho no Shutter Chance (PSVita)
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
        {18279639311550860193u},    // FDAE531AAB414BA1

        // One Piece Dance Battle (iOS/Android)
        {1905818},                  // 00000000001D149A

        // Derby Stallion Masters (iOS/Android)
        {19840202},                 // 00000000012EBCCA

        // World Chain (iOS/Android)
        {4892292804961027794},      // 43E4EA62B8E6C6D2

        // Yuuki Yuuna wa Yuusha de aru - Hanayui no Kirameki / Yuyuyui (iOS/Android)
        {4867249871962584729},      // 438BF1F883653699

        // Tekken Mobile (iOS/Android)
        {18446744073709551615u},    // FFFFFFFFFFFFFFFF

        // Tales of the Rays (iOS/Android)
        {9516284},                  // 00000000009134FC

        // Skylock - Kamigami to Unmei no Itsutsuko (iOS/Android)
        {49160768297},              // 0000000B7235CB29

        // Tokyo Ghoul: Re Invoke (iOS/Android)
        {6929101074247145},         // 00189DFB1024ADE9

        // Azur Lane (iOS/Android)
        {621561580448882},          // 0002354E95356C72

        // One Piece Treasure Cruise (iOS/Android)
        {1224},                     // 00000000000004C8

        // Schoolgirl Strikers ~Twinkle Melodies~ (iOS/Android)
        {15806334760965177344u},    // DB5B61B8343D0000

        // Bad Apple Wars (PSVita)
        {241352432},                // 000000000E62BEF0

        // Koi to Senkyo to Chocolate Portable (PSP)
        {243812156},                // 000000000E88473C

        // Custom Drive (PSP)
        {2012062010},               // 0000000077EDA13A

        // Root Letter (PSVita)
        {1547531215412131},         // 00057F78B05F9BA3

        // Pro Evolution Soccer 2018 / Winning Eleven 2018 (Android)
        {14121473},                 // 0000000000D77A01

        // Kirara Fantasia (Android/iOS)
        {51408295487268137},        // 00B6A3928706E529

        // A3! (iOS/Android)
        {914306251},                // 00000000367F34CB

        // Weekly Shonen Jump: Ore Collection! (iOS/Android)
        {11708691},                 // 0000000000B2A913

        // Monster Gear Versus (iOS/Android)
        {12818105682118423669u},    // B1E30F346415B475

        // Yumeiro Cast (iOS/Android)
        {14418},                    // 0000000000003852

        // Ikki Tousen: Straight Striker (iOS/Android)
        {1000},                     // 00000000000003E8

        // Zero kara Hajimeru Mahou no Sho (iOS/Android)
        {15197457305692143616u},    // D2E836E662F20000

        // Soul Reverse Zero (iOS/Android)
        {2873513618},               // 00000000AB465692

        // Jojo's Bizarre Adventure: Diamond Records (iOS/Android) [additional data]
        {9368070542905259486u},     // 820212864CAB35DE

        // HUNTER x HUNTER: World Hunt (iOS/Android)
        {71777214294589695},        // 00FF00FF00FF00FF

        // \Comepri/ Comedy Prince (iOS/Android)
        {201537197868},             // 0000002EEC8D972C

        // Puzzle of Empires (iOS/Android)
        {13687846},                 // 0000000000D0DC26

        // Aozora Under Girls! (iOS/Android)
        {4988006236073},            // 000004895C56FFA9

        // Castle & Dragon (iOS/Android)
        {20140528},                 // 00000000013351F0

        // Uta no Prince sama Shining Live (iOS/Android)
        {2122831366},               // 000000007E87D606

        // Sevens Story (iOS/Android)
        {629427372852},             // 000000928CCB8334

        // MinGol: Everybody's Golf (iOS/Android)
        {1430028151061218},         // 0005149A5FF67AE2

        // AKB48 Group Tsui ni Koushiki Otoge demashita. (iOS/Android)
        {831021912315111419},       // 0B886206BC1BA7FB

        // Sen no Kaizoku (iOS/Android)
        {81368371967},              // 00000012F1EED2FF

        // I Chu (iOS/Android)
        {13456},                    // 0000000000003490

        // Shinobi Nightmare (iOS/Android)
        {369481198260487572},       // 0520A93135808594

        // Bungo Stray Dogs: Mayoi Inu Kaikitan (iOS/Android)
        {1655728931134731873},      // 16FA54B0C09F7661

        // Super Sentai Legend Wars (iOS/Android)
        {4017992759667450},         // 000E4657D7266AFA

        // Metal Saga: The Ark of Wastes (iOS/Android)
        {100097101118097115},       // 01639DC87B30C6DB

        // Taga Tame no Alchemist (iOS/Android)
        {5047159794308},            // 00000497222AAA84

        // Shin Tennis no Ouji-sama: Rising Beat (iOS/Android) voices?
        // UNI'S ON AIR (iOS/Android)
        {4902201417679},            // 0000047561F95FCF

        // Kai-ri-Sei Million Arthur (Vita)
        {1782351729464341796},      // 18BC2F7463867524

        // Dx2 Shin Megami Tensei Liberation (iOS/Android)
        {118714477},                // 000000000713706D

        // Oira (Cygames) [iOS/Android]
        {46460622},                 // 0000000002C4EECE

        // Dragon Ball Legends (Bandai Namco) [iOS/Android]
        {7335633962698440504},      // 65CD683924EE7F38

        // Princess Connect Re:Dive (iOS/Android/PC)
        {3201512},                  // 000000000030D9E8

        // PriPara: All Idol Perfect Stage (Takara Tomy) [Switch]
        {217735759},                // 000000000CFA624F

        // Space Invaders Extreme (Taito Corporation, Backbone Entertainment) [PC]
        {91380310056},              // 0000001546B0E028

        // CR Another God Hades Advent (Universal Entertainment Corporation) [iOS/Android]
        {64813795},                 // 0000000003DCFAE3

        // Onsen Musume: Yunohana Kore Kushon (Android) voices
        {6667},                     // 0000000000001A0B

        /* Libra of Precatus (Android) */
        {7894523655423589588},      // 6D8EFB700870FCD4

        /* Mashiro Witch (Android) */
        {6183755869466481156},      // 55D11D3349495204

        /* Iris Mysteria! (Android) */
        {62049655719861786},        // 00DC71D5479E1E1A

        /* Kotodaman (Android) */
        {19850716},                 // 00000000012EE5DC

        /* Puchiguru Love Live! (Android) */
        {355541041372},             // 00000052C7E5C0DC

        /* Dolls Order (Android) */
        {153438415134838},          // 00008B8D2A3AA076

        /* Fantasy Life Online (Android) */
        {123456789},                // 00000000075BCD15

        /* Wonder Gravity (Android) */
        {30623969886430861},        // 006CCC569EB1668D

        /* Ryu ga Gotoku Online (Android) */
        {59361939},                 // 000000000389CA93

        /* Sengoku BASARA Battle Party (Android) */
        {836575858265},             // 000000C2C7CE8E59

        /* DAME x PRINCE (Android) */
        {217019410378917901},       // 030302010100080D

        /* Uta Macross SmaPho De Culture (Android) */
        {396798934275978741},       // 0581B68744C5F5F5

        /* Touhou Cannonball (Android) */
        {5465717035832233},         // 00136B0A6A5D13A9

        /* Love Live! School idol festival ALL STARS (Android) */
        {6498535309877346413},      // 5A2F6F6F0192806D

        /* BLACKSTAR -Theater Starless- (Android) */
        {121837007188},             // 0000001C5E0D3154

        /* Nogizaka46 Rhythm Festival (Android) */
        {5613126134333697},         // 0013F11BC5510101

        /* IDOLiSH7 (Android) */
        {8548758374946935437},      // 76A34A72E15B928D

        /* Phantom of the Kill (Android) */
        {33624594140214547},        // 00777563E571B513

        /* Dankira!!! Boys, be DANCING! (Android) */
        {3957325206121219506},      // 36EB3E4EE38E05B2

        /* Idola: Phantasy Star Saga (Android) */
        {12136065386219383975u},    // A86BF72B4C852CA7

        /* Arca Last (Android) */
        {612310807},                // 00000000247F1F17

        /* ArkResona (Android) */
        {564321654321},             // 0000008364311631

        /* Kemono Friends 3 (Android) */
        {3315495188},               // 00000000C59E7114

        /* Inazuma Eleven SD (Android) */
        {14138734607940803423u},    // C436E03737D55B5F

        /* Detective Conan Runner / Case Closed Runner (Android) */
        {1175268187653273344},      // 104f643098e3f700

        /* I Chu EtoileStage (Android) */
        {1433227444226663680},      // 13E3D8C45778A500

        /* 22/7 Ongaku no Jikan (Android) */
        {20190906},                 // 00000000013416BA

        /* Cardcaptor Sakura: Happiness Memories (Android) */
        {625144437747651},          // 00023890C8252FC3

        /* Digimon Story: Cyber Sleuth (PC) */
        {2897314143465725881},      // 283553DCE3FD5FB9

        /* Alice Re:Code (Android) */
        {9422596198430275382u},     // 82C3C951C561F736

        /* Tokyo 7th Sisters (Android) */
        {18279639311550860193u},    // FDAE531AAB414BA1

        /* High School Fleet: Kantai Battle de Pinch! (Mobile) */
        {43472919336422565},        // 009A7263CA658CA5

        /* Disney's Twisted Wonderland (Android) */
        {2895000877},               // 00000000AC8E352D 

        /* B-PROJECT Kaikan Everyday (Android) */
        {12316546176516217334u},    // AAED297DDEF1D9F6

        /* HELIOS Rising Heroes (Android) */
        {311981570940334162},       // 04546195F85DF052

        /* World Ends's Club (iOS) */
        {50979632184989243},        // 00B51DB4932A963B

        /* Kandagawa Jet Girls (PC) */
        {6235253715273671},         // 001626EE22C887C7

        /* Re:Zero - Lost in Memories (Android) */
        {1611432018519751642},      // 165CF4E2138F7BDA

        /* D4DJ Groovy Mix (Android) [base files] */
        {393410674916959300},       // 0575ACECA945A444

        /* Toji no Miko: Kizamishi Issen no Tomoshibi (Android) */
        {62057514034227932},        // 00DC78FAEFA76ADC

        /* Readyyy! (Android) */
        {1234567890987654321},      // 112210F4B16C1CB1

        /* Dragalia Lost (iOS/Android) */
        {2967411924141,         subkeys_dgl, sizeof(subkeys_dgl) / sizeof(subkeys_dgl[0]) },    // 000002B2E7889CAD

};

#endif/*_HCA_KEYS_H_*/
