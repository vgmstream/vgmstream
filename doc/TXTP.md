# TXTP FORMAT

TXTP is a text file with commands, to improve support for games using audio in certain uncommon or undesirable ways. It's in the form of a mini-playlist or a wrapper with play settings.

Simply create a file named `(filename).txtp`, and inside write the commands described below.


## TXTP FEATURES

### Play separate intro + loop files together as a single track
Some games clumsily loop audio by using multiple full file "segments":
 
__Ratchet & Clank (PS2)__: _bgm01.txtp_
```
# define 2 or more segments to play as one
BGM01_BEGIN.VAG
BGM01_LOOPED.VAG

# segments must define loops
loop_start_segment = 2 # 2nd file start
loop_end_segment = 2 # optional, default is last
```
Channel number must be equal, mixing sample rates is ok (uses first).

If your loop segment has proper loops you want to keep, you can use:
```
BGM_SUMMON_0001_01-Intro.hca
BGM_SUMMON_0001_01-Intro2.hca
BGM_SUMMON_0001_01.hca

loop_start_segment = 3
loop_mode = keep  # loops in 3rd file's loop_start to 3rd file's loop_end
```
```
bgm_intro.adx
bgm_main.adx
bgm_main2.adx

loop_start_segment = 2
loop_end_segment = 3
loop_mode = keep    # loops in 2nd file's loop_start to 3rd file's loop_end
```

### Multilayered songs
TXTP "layers" play songs with channels/parts divided into files as one (for example main melody + vocal track).

__Nier Automata__: _BGM_0_012_song2.txtp_
```
# mix dynamic sections (2ch * 2)
BGM_0_012_04.wem
BGM_0_012_07.wem

mode = layers
```

__Life is Strange__: _BIK_E1_6A_DialEnd.txtp_
```
# bik multichannel isn't autodetectable so must mix manually (1ch * 3)
BIK_E1_6A_DialEnd_00000000.audio.multi.bik#1
BIK_E1_6A_DialEnd_00000000.audio.multi.bik#2
BIK_E1_6A_DialEnd_00000000.audio.multi.bik#3

mode = layers
```
Note that the number of channels is the sum of all layers, so three 2ch layers play as a 6ch file.


### Minifiles for bank formats without splitters
__Super Robot Taisen OG Saga - Masou Kishin III - Pride of Justice (Vita)__: _bgm_12.txtp_
```
# select subsong 12
bigfiles/bgm.sxd2#12 #relative paths are ok too for TXTP

#bigfiles/bgm.sxd2#s12 # "sN" is alt for subsong

# single files loop normally by default
# if loop segment is defined it forces a full loop (0..num_samples)
#loop_start_segment = 1
```

### Play segmented subsongs as one
__Prince of Persia Sands of Time__: _song_01.txtp_
```
# can use ranges ~ to avoid so much C&P
amb_fx.sb0#254
amb_fx.sb0#122~144
amb_fx.sb0#121 #notice "#" works as config or comment

#3rd segment = subsong 123, not 3rd subsong
loop_start_segment = 3
```


### Channel mask for channel subsongs/layers
__Final Fantasy XIII-2__: _music_Home_01.ps3.txtp_
```
#plays channels 1 and 2 = 1st subsong
music_Home.ps3.scd#c1,2
```

__Final Fantasy XIII-2__: _music_Home_02.ps3.txtp_
```
#plays channels 3 and 4 = 2nd subsong
music_Home.ps3.scd#c3,4

# song still has 4 channels, just mutes some
```


### Custom play settings
Those setting should override player's defaults if set (except "loop forever"). They are equivalent to some test.exe options.

__God Hand (PS2)__: _boss2_3ningumi_ver6.txtp_ (each line is a separate TXTP)
```
# set number of loops
boss2_3ningumi_ver6.adx#l3

# set fade time (in seconds)
boss2_3ningumi_ver6.adx#f10.5

# set fade delay (in seconds)
boss2_3ningumi_ver6.adx#d0.5

# ignore and disable loops
boss2_3ningumi_ver6.adx#i

# don't fade out and instead play the song ending after
boss2_3ningumi_ver6.adx#F  # this song has a nice stop

# force full loops from end-to-end
boss2_3ningumi_ver6.adx#E

# settings can be combined
boss2_3ningumi_ver6.adx#l2#F  # 2 loops + ending

# settings can be combined
boss2_3ningumi_ver6.adx#l1.5#d1#f5

# boss2_3ningumi_ver6.adx#l1.0#F  # this is equivalent to #i
```

For segments and layers the first file defines looping options.


### Force sample rate
A few games set a sample rate value in the header but actually play with other (applying some of pitch or just forcing it)

__Super Paper Mario (Wii)__
```
btl_koopa1_44k_lp.brstm#h22050  #in hz
```
__Patapon (PSP)__
```
ptp_btl_bgm_voice.sgd#s1#h11050
```


### Force plugin extensions
vgmstream supports a few common extensions that confuse plugins, like .wav/ogg/aac/opus/etc, so for them those extensions are disabled and are expected to be renamed to .lwav/logg/laac/lopus/etc. TXTP can make plugins play those disabled extensions, since it calls files directly by filename.

Combined with TXTH, this can also be used for extensions that aren't normally accepted by vgmstream.


### TXTP combos
TXTP may even reference other TXTP, or files that require TXTH, for extra complex cases. Each file defined in TXTP is internally parsed like it was a completely separate file, so there is a bunch of valid ways to mix them.


### Default commands
You can set defaults that apply to the *resulting* file. This has subtle differences vs per-file config:
```
BGM01_BEGIN.VAG
BGM01_LOOPED.VAG

# force looping from begin to end of the whole thing
commands = #E
```
```
# mix 2ch * 2
BGM_0_012_04.wem
BGM_0_012_07.wem
mode = layers

# plays R of BGM_0_012_04 and L of BGM_0_012_07
commands = #c2,3
```

As it applies at the end, some options with ambiguous or technically hard to handle meanings may be ignored:
```
bgm.sxd2
bgm.sxd2

# ignored (resulting file has no subsongs, should apply to all?)
commands = #s12
```


## TXTP PARSING ISSUES
*Commands* can be chained, but must not be separated by a space (everything after space may be ignored):
```
bgm bank.sxd2#s12#c1,2  #spaces + comment after commands is ignored
```
```
#commands after spaces are seen as comments and ignored
BGM01_BEGIN.VAG    #c1,2
BGM01_LOOPED.VAG   #c1,2
```

However *values* found after *=* allow spaces until value start, and until next space:
```
bgm.sxd2#s12
loop_start_segment   =    1  #spaces surrounding value are ignored
```
```
bgm.sxd2
commands =  #s12#c1,2   #must not have spaces once value starts until end
```
The parser is very simplistic and fairly lax, though may be erratic with edge cases or behave unexpectedly due to unforeseen use-cases and bugs. As filenames may contain spaces or #, certain name patterns could fool it too. Keep in mind this while making .txtp files.


## MINI-TXTP
To simplify TXTP creation, if the .txtp is empty (0 bytes) its filename is used directly as a command. Note that extension is also included (since vgmstream needs a full filename).
- _bgm.sxd2#12.txtp_: plays subsong 12
- _Ryoshima Coast 1 & 2.aix#c1,2.txtp_: channel mask
- _boss2_3ningumi_ver6.adx#l2#F.txtp_: loop twice then play song end file normally
- etc
