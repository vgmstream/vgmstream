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
        // Gunbit (Android)
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

        /* Toji no Miko: Kizamishi Issen no Tomoshibi (Android) */
        {62057514034227932},        // 00DC78FAEFA76ADC

        /* Readyyy! (Android) */
        {1234567890987654321},      // 112210F4B16C1CB1

        /* HoneyWorks Premium Live (Android) */
        {20200401000000},           // 0000125F45B9D640

        /* Assault Lily Last Bullet (Android) */
        {6349046567469313},         // 00168E6C99510101

        /* Sakura Kakumei (iOS/Android) */
        {382759},                   // 000000000005D727

        /* Uma Musume (Android) */
        {75923756697503},           // 0000450D608C479F

        /* D4DJ Groovy Mix (Android) [base files] */
        {393410674916959300},       // 0575ACECA945A444
        /* D4DJ Groovy Mix (Android) [music_* files, per-song later mixed with subkey] */
        {0x59f449354d063308},
        {0x33848be13a2884a3},
        {0xf7e53533d82d48dd},
        {0x244a92885ab77b7c},
        {0xce0796d2a956dc5a},
        {0x73667711348f833f},
        {0x8032f83cbf0076a1},
        {0x7a708e291692abb9},
        {0x9ebb560685327081},
        {0x065c2f8500bc12c8},
        {0x73621a0d321e60c2},
        {0xfcce3164db70522d},
        {0xf4093992cadd3708},
        {0xf965a1086b3179c3},
        {0x54aaada4a1b8deef},
        {0xd4d2a706a06377ef},
        {0x0de4959221bc2675},
        {0x2ecdf66c680f3a45},
        {0x24c0b49097e9ebff},
        {0xc28331aab2612584},
        {0x6750f4d05183bc01},
        {0xda65af760e02c6ee},
        {0x217782495c8b2972},
        {0xbf7712e175c0b265},
        {0x3804d53c43293080},
        {0xd0ed8e940d8ed705},
        {0xf74cf8d4a5d008ce},
        {0xeb8aac34dc178f65},
        {0xbf5902d516db6ed5},
        {0xad071dce0c070e65},
        {0x56ecfc7ef4c65be8},
        {0xf42d31b5ecd1aec1},
        {0x0a8ee7a3a20ce822},
        {0xb9cedc6d6738d481},
        {0xdc2680acfd1b9b64},
        {0x9fbd8a172d5ba3e3},
        {0xb65d86624a857788},
        {0x8f5f05c835f7280e},
        {0x55912db4388961ac},
        {0x4ba9a9471f49b74e},
        {0x6ba36cadf1e045cf},
        {0x230c9509bbc3df0d},
        {0x7148dda3afa76439},
        {0xa6cefd4472568948},
        {0xfe31517282d40690},
        {0x0a6a15cc9722257d},
        {0x447d08ca3148599d},
        {0xb30acd0a43754e5c},
        {0xc05f8e4ea8c3e487},
        {0x8463554672bfb716},
        {0x0d40ccba5e10385a},
        {0xeeaf8d2458ccdb36},
        {0x0d80d3dcc7c75cea},
        {0x8efa09c6df3991a4},
        {0x6867cc75639ee0c3},
        {0xdf30ed86c3d00ffb},
        {0xf7e11ec9c94402f1},
        {0xdb03ecca6a0151e2},
        {0x212bbee264be5b06},
        {0x87025d78a57af15b},
        {0x6139edfb8889032d},
        {0xe67f4da6012c5d24},
        {0x05e3eba376e0b3dd},
        {0xf7edc5d72fdd6ceb},
        {0x031e072678ad18a3},
        {0x290fbc93e184af1e},
        {0xfd3ea450350d666f},
        {0x037d1452c192b1e6},
        {0x15f82c1617013c36},
        {0x5d1f3fdbbb036f8d},
        {0x5089e16d7a676ab1},
        {0x15bb78c31db0a0b6},
        {0xe4a1737fa3d34ccb},
        {0xd2cb7692d690b3a7},
        {0x1bbad843d5971358},
        {0x7ed1fa0b6ec8f9b3},
        {0x529969b7e1e9ac18},
        {0x2f3528a4b9eaa0f7},
        {0x90fefcd350bd2cb8},
        {0xee8da2806a13eecf},
        {0xcd3fb92065d9f373},
        {0x67b89634319c1d36},
        {0x114245b98dcb75bf},
        {0xaff9df030e63e5ba},
        {0x0aebfdf85aae4424},
        {0x4a4462cb0375001e},
        {0xfd9fa5bcb347c01b},
        {0x7ce69eed81f01019},
        {0xcb3d9329d40490b2},
        {0xe0b8bb03c74bb3d0},
        {0xd9a00c9bc93014a8},
        {0x84dc42f5a05f77cf},
        {0x420d4dd413053980},
        {0xff7640b46d72b337},
        {0x711ef85045b8c26e},
        {0xe553dba6592293d8},
        {0x9ebbaf63ffe9d9ef},
        {0x00e978d394512cfd},
        {0x6f9735c02faf6aae},
        {0x8258ddd6a1d0849b},
        {0xe5e83d31e64273f8},
        {0x35128087963cd5be},
        {0x9de6ace9a0e62f44},
        {0xd4c36ab962153420},
        {0xe77aa2f3c90a4e84},
        {0x9f6881f6d7a91658},
        {0x6c6c1fd51e28a1e7},
        {0x867d47a7d8376402},
        {0x79c5f00d243e3097},
        {0x8f0e96b4f71f724f},
        {0xcccb5077d978def4},
        {0x8ad213dddedc9022},
        {0x6aa0ff881da270e7},
        {0xf616642579ba5850},
        {0x5205a666f976d42f},
        {0xa139c29e97fcefb4},
        {0xdb402bd08d522f34},
        {0x2f2c0ff3ff235bd6},
        {0xa0316b536c8b7540},
        {0x260a354b925afeaf},
        {0x567d295828f1b08a},
        {0xc24049b9f7ed3105},
        {0x8815d2dffd77a71e},
        {0x2b4a83e7d54d0554},
        {0xf6b0dc07ea8ebeb7},
        {0xbb7be9c7c620f504},
        {0x7465c7c473e53a40},
        {0xa4481f97a8d4d01c},
        {0x0046fd87a21859ac},
        {0xf1db3c1d9542063a},
        {0xaba147637d52efbe},
        {0x298a0fa05c3f355f},
        {0x465e30321a4091f2},
        {0xc40c398f7e80d184},
        {0xa76262c2557be76f},
        {0xaef2954dc3657336},
        {0xa3711cc06f9b86c2},
        {0xcb60232f2f27ace5},
        {0x4c7d7c251c6bfa95},
        {0xf877dea1180b9b90},
        {0x9ce13dcb2fb389cc},
        {0xcbf4f1324081e0a6},
        {0x444dda6d55d76095},
        {0xb2bd99fa559b9062},
        {0x1ed521f6dd691255},
        {0xdfad847a86a126bb},
        {0x2a47feac8dc3ca9c},
        {0xc352bbf3d519256e},
        {0xfdec74b23d8b494b},
        {0x1dd21a1244ca12f1},
        {0xaf9d7a05b0fc3d9e},
        {0xa662be1601e49476},
        {0xd2ce91dbfc209b10},
        {0x57bdc58e4c06fc76},
        {0xe350bffcdc9cb686},
        {0xc7da8e6f0e2fe399},
        {0x984363837811b08a},
        {0xdcd2a403fb01e164},
        {0xb2770dced3cfd9a7},
        {0x0df31e26a7b036a2},
        {0x3f25fe3395b3154c},
        {0x889d47adc9595ffa},
        {0xc04264e8f34ad5c0},
        {0xc222e70e4a79f7c3},
        {0x7c7dd6d9f3761102},
        {0x904f50c5ce8ec6e4},
        {0xb7bff4fbf66be43f},
        {0x776c4aded0bca5d1},
        {0x38ad99a045dc971f},
        {0xb921c3992807dadd},
        {0x68d576c631e61265},
        {0xcede847721873fc2},
        {0x40443974a0a86b8b},
        {0x57111c24801b44a1},
        {0x6a15a9610d10d210},
        {0xb18fb83ee356fb94},
        {0x59b1257242c40109},
        {0x22ef086d7d6ce520},
        {0x76254d1ef50c004c},
        {0x7678588b0adf59df},
        {0x4fffee4065d22bec},
        {0x0dc128f2fd48bf4b},
        {0xfb647d074e53fab6},
        {0x55b7b25821375a02},
        {0x5b877af6e52af19b},
        {0xba26e58923a5da5d},
        {0x52d065d9ccdb8696},
        {0xf0c624dc0385adae},
        {0xb7a5297198a73155},
        {0xda08e9d3961c93f2},
        {0x8328668369631cc1},
        {0xb140168a47d55b92},
        {0x6699616be2c50115},
        {0xcee66d585d689851},
        {0x5771a2c76f36c898},
        {0x5e91a3790c32e2b3},
        {0xe4e11a71fe620e3a},
        {0x1bb363adcf4eb3f8},
        {0xa691936caf4d91d0},
        {0x94466db0d3c10f4b},
        {0x47f52330df2ead11},
        {0x33848be13a2884a3},
        {0xc9f159f60b065f91},
        {0xdd9ca800a7123d6f},
        {0xa090c8ebf8463d05},
        {0xa5c1adeb7919845f},
        {0x58d97e6f3d1aee86},
        {0x71b5fa3761d6726d},
        {0x1980271cfe0da9bd},
        {0x945cdb3cf1f29e52},
        {0x7f0feac6be7def5b},

        /* Dragalia Lost (iOS/Android) */
        {2967411924141,         subkeys_dgl, sizeof(subkeys_dgl) / sizeof(subkeys_dgl[0]) },    // 000002B2E7889CAD

};

#endif/*_HCA_KEYS_H_*/
