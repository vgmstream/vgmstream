#ifndef _API_VERSION_H_
#define _API_VERSION_H_
#include "api.h"
#if LIBVGMSTREAM_ENABLE

/* Current API version.
 * - only refers to the API itself, as changes related to formats/etc don't alter this (since they are usually additive)
 * - vgmstream's features are mostly stable, but this API may be tweaked from time to time
 */
#define LIBVGMSTREAM_API_VERSION_MAJOR 1    // breaking API/ABI changes
#define LIBVGMSTREAM_API_VERSION_MINOR 0    // compatible API/ABI changes
#define LIBVGMSTREAM_API_VERSION_PATCH 0    // fixes

/* returns API version in hex format: 0xMMmmpppp = MM-major, mm-minor, pppp-patch
 * - use when loading vgmstream as a dynamic library to ensure API/ABI compatibility
 */
LIBVGMSTREAM_API uint32_t libvgmstream_get_version(void);

/* CHANGELOG:
 *
 * - 1.0.0: initial version
 */

#endif
#endif
