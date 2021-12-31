# vgmstream
This is vgmstream, a library for playing streamed (prerecorded) video game audio.

Some of vgmstream's features:
- [Hundreds of video game music formats and codecs](doc/FORMATS.md), from typical game engine files
  to obscure single-game codecs, aiming for high accuracy and compatibility.
- Support for looped BGM, using file's internal metadata for smooth transitions, with accurate
  sample counts.
- [Subsongs](doc/USAGE.md#subsongs), playing a format's multiple internal songs separately.
- Many types of companion files (data split into multiple files) and custom containers.
- Encryption keys, internal stream names, and many other unusual cases found in game audio.
- [TXTH](doc/TXTH.md) function, to add external support for extra formats, including raw audio in
  many forms.
- [TXTP](doc/TXTP.md) function, for real-time and per-file config, like forced looping, removing
  channels, playing certain subsong, or fusing multiple files into a single one.
- Simple [external tagging](doc/USAGE.md#tagging) via .m3u files.
- [Plugins](#getting-vgmstream) are available for various media player software and operating systems.

The main development repository: https://github.com/vgmstream/vgmstream/

Automated builds with the latest changes: https://vgmstream.org/downloads

Common releases: https://github.com/vgmstream/vgmstream/releases

Help can be found here: https://www.hcs64.com/

More documentation: https://github.com/vgmstream/vgmstream/tree/master/doc

## Getting vgmstream
There are multiple end-user components:
- [test.exe/vgmstream-cli](doc/USAGE.md#testexevgmstream-cli-command-line-decoder): A command-line decoder.
- [in_vgmstream](doc/USAGE.md#in_vgmstream-winamp-plugin): A Winamp plugin.
- [foo_input_vgmstream](doc/USAGE.md#foo_input_vgmstream-foobar2000-plugin): A foobar2000 component.
- [xmp-vgmstream](doc/USAGE.md#xmp-vgmstream-xmplay-plugin): An XMPlay plugin.
- [vgmstream.so](doc/USAGE.md#audacious-plugin): An Audacious plugin.
- [vgmstream123](doc/USAGE.md#vgmstream123-command-line-player): A command-line player.

The main library (plain *vgmstream*) is the code that handles the internal conversion, while the
above components are what you use to get sound.

If you just want to convert game audio to `.wav`, easiest would be getting *test.exe/vgmstream-cli* (see
below) then drag-and-drop one or more files to the executable. This should create `(file.extension).wav`,
if the format is supported. More usable would be installing a music player like *foobar2000* (for
Windows) or *Audacious* (for Linux) then the appropriate component, so you can listen to VGM without
converting and set options like infinite looping.

See [components](doc/USAGE.md#components) in the *usage guide* for full install instructions and
explanations. The aim is feature parity, but there are a few differences between them due to
missing implementation on vgmstream's side or lack of support in target player or API.

Note vgmstream cannot *encode* (convert from `.wav` to some video game format), it only *decodes*
(plays game audio).


### Windows
You should get `vgmstream-win.zip`, which also bundles various components, or
`foo_input_vgmstream.fb2k-component` for the installable foobar2000 plugin from the
latest prebuilt binaries on our website:
- https://vgmstream.org/downloads

You can also get them from the less frequently updated releases on GitHub:
- https://github.com/vgmstream/vgmstream/releases

If the above links fail, you may also try the alternative, somewhat recent versions built by
[bnnm](https://github.com/bnnm):
- https://github.com/bnnm/vgmstream-builds/raw/master/bin/vgmstream-latest-test-u.zip

If you prefer, you may compile the components from source as well, see the
[build guide](doc/BUILD.md) for more information.

### Linux
For convenience, releases distribute a command-line decoder in `vgmstream-cli.zip`. It is
statically linked and should work on all systems running Linux kernel v3.2 and above.
- https://vgmstream.org/downloads
- https://github.com/vgmstream/vgmstream/releases

Building from source will also give you *vgmstream.so*, an Audacious plugin, and *vgmstream123*,
a command-line player.

When building from source code, many components have to be installed or compiled separately. The
[build guide](doc/BUILD.md) describes this process in more detail. For a quick build on Debian and
Ubuntu-style distributions, run `./make-build-cmake.sh`. The script will be installing various
dependencies, so you may prefer to copy the commands from the file and run them one by one.

### macOS
Please follow the [build guide](doc/BUILD.md).


## More info
- [Usage guide](doc/USAGE.md)
- [List of supported audio formats](doc/FORMATS.md)
- [Build guide](doc/BUILD.md)
- [TXTH file format](doc/TXTH.md)
- [TXTP file format](doc/TXTP.md)


Enjoy! *hcs*
