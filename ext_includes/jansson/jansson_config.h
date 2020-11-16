/*
 * Copyright (c) 2010-2016 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 *
 *
 * This file specifies a part of the site-specific configuration for
 * Jansson, namely those things that affect the public API in
 * jansson.h.
 *
 * The CMake system will generate the jansson_config.h file and
 * copy it to the build and install directories.
 */

#ifndef JANSSON_CONFIG_H
#define JANSSON_CONFIG_H

/* Define this so that we can disable scattered automake configuration in source files */
#ifndef JANSSON_USING_CMAKE
#define JANSSON_USING_CMAKE
#endif


/* If your compiler supports the inline keyword in C, JSON_INLINE is
   defined to `inline', otherwise empty. In C++, the inline is always
   supported. */
#if defined(_MSC_VER) && (_MSC_VER < 1800) && !defined(__cplusplus)
#define JSON_INLINE __inline
#else
#define JSON_INLINE inline
#endif


#define json_int_t long
#define json_strtoint strtol
#define JSON_INTEGER_FORMAT "ld"


/* If locale.h and localeconv() are available, define to 1, otherwise to 0. */
#define JSON_HAVE_LOCALECONV 1

/* If __atomic builtins are available they will be used to manage
   reference counts of json_t. */
#define JSON_HAVE_ATOMIC_BUILTINS 0

/* If __atomic builtins are not available we try using __sync builtins
   to manage reference counts of json_t. */
#define JSON_HAVE_SYNC_BUILTINS 0

/* Maximum recursion depth for parsing JSON input.
   This limits the depth of e.g. array-within-array constructions. */
#define JSON_PARSER_MAX_DEPTH 2048

#endif
