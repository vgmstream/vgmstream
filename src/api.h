#ifndef _API_H_
#define _API_H_
#include "base/plugins.h" //TODO: to be removed

#if 0

/* vgmstream's public API
 * basic usage (also see api_example.c):
 *   - libvgmstream_get_version()   // if needed
 *   - libvgmstream_init(...)       // base context
 *   - libvgmstream_setup(...)      // standard config
 *   - libvgmstream_open(...)       // check detected format
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


/* Current API version. Only refers to the API itself, as changes related to formats/etc don't alter it.
 * vgmstream's features are mostly stable, while this API may change from time to time.
 * May change as well when related (such as api_streamfile.h) are changed. */
#define LIBVGMSTREAM_API_VERSION_MAJOR 1    // breaking API/ABI changes
#define LIBVGMSTREAM_API_VERSION_MINOR 0    // compatible API/ABI changes
#define LIBVGMSTREAM_API_VERSION_PATCH 0    // fixes

#include "api_main.h"
#include "api_streamfile.h"
#include "api_tags.h"

#endif
#endif
