/* Copyright (C) 2002-2003 Jean-Marc Valin
 Copyright (C) 2007-2009 Xiph.Org Foundation */
/**
 @file stack_alloc.h
 @brief Temporary memory allocation on stack
 */
/*
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:
 
 - Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 
 - Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef STACK_ALLOC_H
#define STACK_ALLOC_H

#if (!defined (VAR_ARRAYS) && !defined (USE_ALLOCA))
#error "Vgmstream requires one of VAR_ARRAYS or USE_ALLOCA be defined to select the temporary allocation mode."
#endif

#ifdef USE_ALLOCA
# ifdef WIN32
#  include <malloc.h>
# else
#  ifdef HAVE_ALLOCA_H
#   include <alloca.h>
#  else
#   include <stdlib.h>
#  endif
# endif
#endif

/**
 * @def ALIGN(stack, size)
 *
 * Aligns the stack to a 'size' boundary
 *
 * @param stack Stack
 * @param size  New size boundary
 */

/**
 * @def PUSH(stack, size, type)
 *
 * Allocates 'size' elements of type 'type' on the stack
 *
 * @param stack Stack
 * @param size  Number of elements
 * @param type  Type of element
 */

/**
 * @def VARDECL(var)
 *
 * Declare variable on stack
 *
 * @param var Variable to declare
 */

/**
 * @def ALLOC(var, size, type)
 *
 * Allocate 'size' elements of 'type' on stack
 *
 * @param var  Name of variable to allocate
 * @param size Number of elements
 * @param type Type of element
 */

#if defined(VAR_ARRAYS)

#define VARDECL(type, var)
#define ALLOC(var, size, type) type var[size]
#define SAVE_STACK
#define RESTORE_STACK
#define ALLOC_STACK

#elif defined(USE_ALLOCA)

#define VARDECL(type, var) type *var

# ifdef WIN32
#  define ALLOC(var, size, type) var = ((type*)_alloca(sizeof(type)*(size)))
# else
#  define ALLOC(var, size, type) var = ((type*)alloca(sizeof(type)*(size)))
# endif

#define SAVE_STACK
#define RESTORE_STACK
#define ALLOC_STACK

#endif /* VAR_ARRAYS */

#endif /* STACK_ALLOC_H */
