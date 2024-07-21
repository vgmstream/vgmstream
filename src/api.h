#ifndef _API_H_
#define _API_H_
#include "base/plugins.h" //TODO: to be removed

//#define LIBVGMSTREAM_ENABLE 1
#if LIBVGMSTREAM_ENABLE

/* vgmstream's public API
 *
 * By default vgmstream behaves like a simple decoder (extract samples until stream end), but you can configure it
 * to loop N times or even downmix. In other words, it also behaves a bit like a player.
 * 
 * It exposes multiple options and convenience functions beyond simple decoding mainly for various plugins,
 * since it was faster moving shared behavior to core rather than reimplementing every time.
 * 
 * All this may make the API a bit twisted and coupled (sorry, tried my best), probably will improve later. Probably.
 * 
 * Notes:
 * - vgmstream may dynamically allocate stuff as needed (not too much beyond some setup buffers, but varies per format)
 * - previously the only way to use vgmstream was accesing its internals. Now there is an API internals may change in the future
 * - some details described in the API may not happen at the moment (they are defined for future internal changes)
 * - main reason it uses the slighly long-winded libvgmstream_* names is that internals use the vgmstream_* 'namespace'
 * - c-strings should be in UTF-8
 * - the API is still WIP and may be slightly buggy overall due to lack of time, to be improved later
 * - vgmstream's features are mostly stable, but this API may be tweaked from time to time (check API_VERSION)
 *
 * Basic usage (also see api_example.c):
 *   - libvgmstream_init(...)       // base context
 *   - libvgmstream_setup(...)      // config if needed
 *   - libvgmstream_open(...)       // setup format
 *   - libvgmstream_play(...)       // main decode
 *   - output samples + repeat libvgmstream_play until stream is done
 *   - libvgmstream_free(...)       // cleanup
 */

#include <stdint.h>
#include <stdbool.h>

/* standard C param call and name mangling (to avoid __stdcall / .defs) */
//#define LIBVGMSTREAM_CALL __cdecl //needed?
//LIBVGMSTREAM_API (type) LIBVGMSTREAM_CALL libvgmstream_function(...);

/* define external function behavior (during compilation) */
#if defined(LIBVGMSTREAM_EXPORT)
    #define LIBVGMSTREAM_API __declspec(dllexport) /* when exporting/creating vgmstream DLL */
#elif defined(LIBVGMSTREAM_IMPORT)
    #define LIBVGMSTREAM_API __declspec(dllimport) /* when importing/linking vgmstream DLL */
#else
    #define LIBVGMSTREAM_API /* nothing, internal/default */
#endif

#include "api_version.h"
#include "api_decode.h"
#include "api_helpers.h"
#include "api_streamfile.h"
#include "api_tags.h"

#endif
#endif
