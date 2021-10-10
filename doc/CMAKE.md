# CMake build help

## Build requirements

**CMake**: needs v3.6 or later
- https://cmake.org/download/

**Git**: optional, for version numbers
- Windows: https://git-scm.com/download/win

**GCC / Make** 
- Windows: https://sourceforge.net/projects/mingw-w64/

**MSVC / Visual Studio**
- Windows: https://www.visualstudio.com/downloads/

If building the CLI for *nix-based OSes, vgmstream123 also needs the following:
- **LibAO**

If building for *nix-based OSes, the following libraries are optional:
- **libmpg123**
- **libvorbis** (really libvorbisfile, though)
- **FFmpeg**

See [BUILD.md](BUILD.md)'s *Compilation requirements* for more info about various component and installation.


## Build directions

It is recommended to do out-of-source builds as opposed to in-source builds. Out-of-source builds have been tested to work, while in-source builds have not been tested at all.

Create a directory called `build` and run cmake commands from there.

***NOTE:*** The CMake scripts attempt to collect all the source files at configuration time. If you are following vgmstream development through git or adding your own source files, you **MUST** re-run CMake manually to regenerate the files. Failure to do so can result in either missing functionality or compile errors.

First you will need to run CMake to generate the build setup. You can use either the CMake GUI or run CMake from the command line.

### CMake GUI

If you have access to the CMake GUI, you can use that to create your build setup. Select where the source code is (that should be the directory just above this one) and where you wish to build to (preferably a directory outside source).

You may have to add some entries before configuring will succeed. See the [CMake Cache Entries](#cmake-cache-entries) section for details on the entries.

Once the entries are in place, you can click on Generate (or Configure then Generate). This should ask you what build system you wish to use. As long as there are no errors, you should see the following at the bottom of the window:

```
Configuring done
Generating done
```

Before that you'll see what options are enabled and disabled, what is going to be built and where they will be installed to.

You may need to select a Generator first, depending on your installed tools (for example, Visual Studio 16 2019 or MingW Make on Windows). If you need to change it later, select *File > Delete Cache*. You may need to include those tools in the *Path* variable, inside *Environment...* options.

If you decided to build for a project-based GUI, you can click on Open Project to open that. (NOTE: Only Visual Studio has been tested as a project-based GUI.) If you decided to build for a command line build system, you can open up the command line for the build directory and run your build system.

### CMake command line

If you don't have access to the CMake GUI or would prefer to only use the command line, you can run CMake from there. Navigate to the directory you wish to build to and run the following:

```
cmake -G "<generator>" <options> <path to source code>
```

Replace `<generator>` with the CMake generator you wish to use as your build system (for example `Unix Makefiles`, or don't set for default). Make note of the quotes, and use `cmake -h` to get a list of generators for your system.

You may have to add some entries before configuring will success. See the [CMake Cache Entries](#cmake-cache-entries) section for details on the entries.

Place your entries in the `<options>` section of the above command, with each option in the form of `-D<optionname>:<type>=<value>`. Replace `<path to source code>` with the path where the source code is (that should be the directory just above this one), may need to se `-S <path to source> -B <output dir>` instead. Example:
```
git clone https://github.com/vgmstream/vgmstream.git
cd vgmstream
cmake -DUSE_FFMPEG=ON -DBUILD_AUDACIOUS=OFF -S . -B build
```

You may need to install appropriate packages first (see [BUILD.md](BUILD.md) for more info), for example:
```
sudo apt-get update
# basic compilation
sudo apt-get install -y gcc g++ make build-essential 
# extra libs
sudo apt-get install -y libmpg123-dev libvorbis-dev libspeex-dev
# extra libs
sudo apt-get install -y libavformat-dev libavcodec-dev libavutil-dev libswresample-dev
# for vgmstream123 and audacious
sudo apt-get install -y libao-dev audacious-dev
# for JSON dumping
sudo apt-get install -y libjansson-dev
# for static builds
sudo apt-get install -y yasm libopus-dev
# actual cmake
sudo apt-get install -y cmake

mkdir -p build
cd build
cmake ..
```

Once you have run the command, as long as there are no errors, you should see the following at the bottom of the window:

```
Configuring done
Generating done
```

Before that you'll see what options are enabled and disabled, what is going to be built and where they will be installed to.

You can now run the build system you chose as your generator above, whether that be a command line build system like Unix Make or a project-based GUI like Eclipse.

### CMake Cache Entries

The following are the various options and cache entries that you can choose to use when building vgmstream.

If not using a project-based GUI, then you will also need to set what build type you want. This can be set with the `CMAKE_BUILD_TYPE` option and takes one of the following values:

- **Debug**
- **Release**
- **RelWithDebInfo**: Like Release but with debugging information included
- **MinSizeRel**: Like Release but aims for minimum size

For example: `cmake .. -DCMAKE_BUILD_TYPE=Release`

#### Library Options

All of these options are of type BOOL and can be set to either `ON` or `OFF`. Most of the details on these libraries can be found in the [External Libraries section of BUILD.md](BUILD.md#external-libraries).

- **USE_MPEG**: Chooses if you wish to use libmpg123 for support of MP1/MP2/MP3. The default is `ON`.
- **USE_VORBIS**: Chooses if you wish to use libvorbis for support of Vorbis. The default is `ON`.
- **USE_FFMPEG**: Chooses if you wish to use FFmpeg for support of many codecs. The default is `ON`. `FFMPEG_PATH` can also be given, so it can use official/external SDK instead of the one used in vgmstream project.
- **USE_MAIATRAC3PLUS**: Chooses if you wish to use MAIATRAC3+ for support of ATRAC3+. The default is `OFF`. It is not recommended to enable.
- **USE_G7221**: Chooses if you wish to use G7221 for support of ITU-T G.722.1 annex C. The default is `ON`.
- **USE_G719**: Chooses if you wish to use libg719_decode for support ITU-T G.719. The default is `ON`.
- **USE_ATRAC9**: Chooses if you wish to use LibAtrac9 for support of ATRAC9. The default is `ON`.
- **USE_SPEEX**: Chooses if you wish to use libspeex for support of SPEEX. The default is `ON`.

The following option is currently only available for **Windows**:

- **USE_CELT**: Chooses if you wish to use libcelt for support of FSB CELT versions 0.6.1 and 0.11.0. The default is `ON`.

The following option is only available for **\*nix-based OSes**:

- **USE_JANSSON**: Chooses if you wish to use libjansson for support of JSON dumping capabilities. The default is `ON`.

#### Build Options

All of these options are of type BOOL and can be set to either `ON` or `OFF`. Example usage: `cmake .. -DBUILD_CLI=ON`

- **BUILD_CLI**: Chooses if you wish to build the vgmstream CLI program. The default is `ON`.

The following options are only available for Windows:

- **BUILD_FB2K**: Chooses if you wish to build the foobar2000 component. Note that this requires `FB2K_SDK_PATH` and `WTL_INCLUDE_PATH` to also be given if this option is `ON`. The default for is `ON`.
- **BUILD_WINAMP**: Chooses if you wish to build the Winamp plugin. The default is `ON`.
- **BUILD_XMPLAY**: Chooses if you wish to build the XMPlay plugin. The default is `ON`.

The following option is only available for *nix-based OSes:

- **BUILD_V123**: Chooses if you wih to build the vgmstream123 player. The default is `ON`.
- **BUILD_AUDACIOUS**: Chooses if you wish to build the Audacious plugin. The default is `ON`.
- **BUILD_STATIC**: Chooses if you wish to build the vgmstream CLI program, statically linking every dependency. Enabling this currently disables building vgmstream123 and the Audacious plugin. The default is `OFF`.

#### Paths

These options are for setting the path to the library source code. The CMake script will then configure and build each library. All of these paths are of type PATH. Example usage: `cmake .. -DMPEG_PATH=~/source_code/mpg123`

If FDK-AAC/QAAC support is enabled, the following paths are required (with more details in the foobar2000 plugin section of [BUILD.md](BUILD.md)):

- **QAAC_PATH**: The path to the QAAC library. It can be obtained at https://github.com/kode54/qaac
- **FDK_AAC_PATH**: The path to the FDK-AAC library. It can be obtained at https://github.com/kode54/fdk-aac
- **MAIATRAC3PLUS_PATH**: The path to the MAIATRAC3+ library. If MAIATRAC3+ support is enabled, providing this path is required. It is not recommended to use.
- **MPEG_PATH**: The path to the mpg123 library. It can be obtained from [the mpg123 project on SourceForge.net](https://sourceforge.net/projects/mpg123/files/mpg123/1.25.10/). If not set and static building is enabled, this will be downloaded automatically.
- **FFMPEG_PATH**: The path to the FFmpeg source directory. It can be obtained at https://git.ffmpeg.org/ffmpeg.git If not set and static building is enabled, this will be downloaded automatically.
- **G719_PATH**: The path to the G.719 decoder library. It can be obtained at https://github.com/kode54/libg719_decode If not set, it is downloaded automatically on Linux.
- **ATRAC9_PATH**: The path to the Atrac9 library. It can be obtained at https://github.com/Thealexbarney/LibAtrac9 If not set, it is downloaded automatically on Linux.
- **SPEEX_PATH**: The path to the SPEEX library. It can be obtained at https://gitlab.xiph.org/xiph/speex If not set, it is downloaded automatically when building with Emscripten.
- **LIBAO_PATH**: The path to the AO library. If static building is enabled and you chose to build the vgmstream123 player, providing this path is required. It is not recommended to use.

The CLI/vgmstream123 programs are normally installed to `CMAKE_INSTALL_PREFIX`, changing this will change where those are installed: `cmake .. -DCMAKE_INSTALL_PREFIX=/custom/path`

If building the foobar2000 component, the following paths are required:

- **FB2K_SDK_PATH**: The path to the foobar2000 SDK.
- **WTL_INCLUDE_PATH**: The path to the include directory of the WTL.
- **FB2K_COMPONENT_INSTALL_PREFIX**: The path to the foobar2000 component installation directory. If you want to install the plugin for all users, you could set this to the `components` directory of the foobar2000 installation. Otherwise you'll want to set this to the `user-components` directory of your foobar2000 folder within your profile, the location of which depends on your current OS. The component as well as the required DLLs will be installed in this directory.

If building the Winamp plugin, the following path is required:

- **WINAMP_INSTALL_PREFIX**: The path to the Winamp installation. The required DLLs will be installed next to `winamp.exe` and the plugin itself will be installed to the `Plugins` directory.

If building the XMPlay plugin, the following path is required:

- **XMPLAY_INSTALL_PREFIX**: The path to the XMPlay installation. The required DLLs and the plugin will be installed in this directory.

If building the Audacious plugin, no path needs to be given, it will be found by CMake.

## Installation

After the above build has been done, the programs and plugins can be installed with CMake as well. For project-based GUIs, running the `INSTALL` target will install the files. For command line build systems, use the `install` target.
