# TXTP FORMAT

TXTP is a text file with commands, to improve support for games using audio in certain uncommon or undesirable ways. It's in the form of a mini-playlist or a wrapper with play settings.

Simply create a file named `(filename).txtp`, and inside write the commands described below.


## TXTP features

### Play separate intro + loop files together as a single track
- __Ratchet & Clank (PS2)__: _bgm01.txtp_
```
# define several files to play as one (there is no limit) 
BGM01_BEGIN.VAG
BGM01_LOOPED.VAG

# multi-files must define loops
loop_start_segment = 2 # 2nd file start
loop_end_segment = 2 # optional, default is last

#channel number must be equal, mixing sample rates is ok (uses first)
```

### Minifiles for bank formats without splitters
- __Super Robot Taisen OG Saga - Masou Kishin III - Pride of Justice (Vita)__: _bgm_12.txtp_
```
# select subsong 12
bigfiles/bgm.sxd2#12 #relative paths are ok too for TXTP

#bigfiles/bgm.sxd2#s12 # "sN" is al alt for subsong

# single files loop normally by default
# if loop segment is defined it forces a full loop (0..num_samples)
#loop_start_segment = 1
```

### Play segmented subsongs as one
- __Prince of Persia Sands of Time__: _song_01.txtp_
```
# can use ranges ~ to avoid so much C&P
amb_fx.sb0#254
amb_fx.sb0#122~144
amb_fx.sb0#121 #notice "#" works as config or comment

#3rd segment = subsong 123, not 3rd subsong
loop_start_segment = 3
```


### Channel mask for channel subsongs/layers
- __Final Fantasy XIII-2__: _music_Home_01.ps3.txtp_
```
#plays channels 1 and 2 = 1st subsong
music_Home.ps3.scd#c1,2
```

- __Final Fantasy XIII-2__: _music_Home_02.ps3.txtp_
```
#plays channels 3 and 4 = 2nd subsong
music_Home.ps3.scd#c3,4

# song still has 4 channels, just mutes some
```


### Multilayered songs

TXTP "layers" play songs with channels/parts divided into files as one

- __Nier Automata__: _BGM_0_012_song2.txtp_
```
# mix dynamic sections (2ch * 2)
BGM_0_012_04.wem
BGM_0_012_07.wem

mode = layers
```

- __Life is Strange__: _BIK_E1_6A_DialEnd.txtp_
```
# bik multichannel isn't autodetectable so must mix manually (1ch * 3)
BIK_E1_6A_DialEnd_00000000.audio.multi.bik#1
BIK_E1_6A_DialEnd_00000000.audio.multi.bik#2
BIK_E1_6A_DialEnd_00000000.audio.multi.bik#3

mode = layers
```


### Channel mapping
TXTP can swap channels for custom channel mappings. Note that channel masking applies after mappings. Format is:
```
#ch1 = first
file1.ext#m2-3  # "FL BL FR BR" to "FL FR BL BR"

#do note the order specified affects swapping
file2.ext#m2-3,4-5,4-6  # ogg "FL CN FR BL BR SB" to wav "FL FR CN SB BL BR"
```


### Custom play settings
Those setting should override player's defaults if set (except "loop forever"). They are equivalent to some test.exe options.

- __God Hand (PS2)__: _boss2_3ningumi_ver6.txtp_ (each line is a separate TXTP)
´´´
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
´´´´

For segments and layers the first file defines looping options.


### Force plugin extensions
vgmstream supports a few common extensions that confuse plugins, like .wav/ogg/aac/opus/etc, so for them those extensions are disabled and are expected to be renamed to .lwav/logg/laac/lopus/etc. TXTP can make plugins play those disabled extensions, since it calls files directly by filename.

Combined with TXTH, this can also be used for extensions that aren't normally accepted by vgmstream.


### TXTP combos
TXTP may even reference other TXTP, or files that require TXTH, for extra complex cases. Each file defined in TXTP is internally parsed like it was a completely separate file, so there is a bunch of valid ways to mix them.


## Mini TXTP

To simplify TXTP creation, if the .txtp is empty (0 bytes) its filename is used directly as a command. Note that extension is also included (since vgmstream needs a full filename).
- _bgm.sxd2#12.txtp_: plays subsong 12
- _Ryoshima Coast 1 & 2.aix#c1,2.txtp_: channel mask
- _boss2_3ningumi_ver6.adx#l2#F.txtp_: loop twice then play song end file normally
- etc


## Other examples

_Join "segments" (intro+body):_
```
#files must have same number of channels
Song001_intro.ogg
Song001_body.ogg
loop_start_segment = 2
```

_Join "layers" (ex. main+vocals):_
```
#files must have same number of samples
Song001_main.ogg
Song001_vocals.ogg
mode = layers
```
