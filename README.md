# vgmstream
This is vgmstream, a library for playing streamed (pre-recorded) audio from
video games.

Some of vgmstream's features:
- hundreds of video game music formats and codecs, from typical game engine files to 
  obscure single-game codecs, aiming for high accuracy and compatibility.
- support for looped BGM, using file's internal metadata for smooth transitions,
  with accurate sample counts
- subsongs, playing a format's multiple internal songs separately
- many types of companion files (data split in multiple files) and custom containers
- encryption keys, internal stream names, and many other unusual cases found in game audio
- TXTH function, to add external support for extra formats (including raw audio in many forms)
- TXTP function, for real-time and per-file config (like forced looping, removing
  channels, playing certain subsong, or fusing together multiple files as a single one)
- simple external tagging via .m3u files
- plugins available for various common players and O.S.

Latest development is here: https://github.com/vgmstream/vgmstream/

Automated builds with the latest changes: https://vgmstream.org/downloads

Common releases: https://github.com/vgmstream/vgmstream/releases

Help can be found here: https://www.hcs64.com/

More docs: https://github.com/vgmstream/vgmstream/tree/master/doc

## Getting vgmstream
There are multiple end-user bits:
- a command line decoder called *test.exe/vgmstream-cli*
- a Winamp plugin called *in_vgmstream*
- a foobar2000 component called *foo_input_vgmstream*
- an XMPlay plugin called *xmp-vgmstream*
- an Audacious plugin called *libvgmstream*
- a command line player called *vgmstream123*

Main lib (plain *vgmstream*) is the code that handles internal conversion, while the
above components are what you use to actually get sound.

See *components* in *usage guide* for install instructions and explanations. The aim
is feature parity, but there are a few differences between them (due to missing
implementation in vgmstream's side, or lack of support in target player/API/etc).

### Windows
You should get `vgmstream-win.zip` (bundle of various components) or
`foo_input_vgmstream.fb2k-component` (installable foobar2000 plugin) from the
latest pre-built binaries:
https://vgmstream.org/downloads

You can also try getting them from the (infrequently updated) releases:
https://github.com/vgmstream/vgmstream/releases

If the above links fail you may try alt, recent-ish versions here:
https://github.com/bnnm/vgmstream-builds/raw/master/bin/vgmstream-latest-test-u.zip

You may compile them from source as well (see *build guide*).

### Linux
Generally you need to build vgmstream's components manually (see *build guide*). For
a quick build call `./make-build-cmake.sh` (for Debian/Ubuntu-style distros, installs
various deps first so you may prefer to call commands manually).

Releases also distribute a static version of the CLI tool (kernel v3.2+).
https://vgmstream.org/downloads
https://github.com/vgmstream/vgmstream/releases

### Mac
Follow the *build guide* instructions. You can probably use Linux's script above with
some tweaks.


## More info
- [Usage guide](doc/USAGE.md)
- [Build guide](doc/BUILD.md)
- [TXTH info](doc/TXTH.md)
- [TXTP info](doc/TXTP.md)


Enjoy! *hcs*
