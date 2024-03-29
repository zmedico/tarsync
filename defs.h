/*
  Copyright (C) 2003-2005 Brian Harring
  Copyright (C) 2021 tarsync contributors

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, US 
*/
#ifndef _HEADER_DEFS
#define _HEADER_DEFS 1

#include <errno.h>

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

extern unsigned int global_verbosity;

typedef unsigned int off_u32;
typedef signed   int off_s32;
#ifdef LARGEFILE_SUPPORT
typedef unsigned long long off_u64;
typedef signed   long long off_s64;
#else
typedef off_u32 off_u64;
typedef off_s32 off_s64;
#endif
typedef unsigned long long act_off_u64;
typedef signed   long long act_off_s64;

#define PATCH_TRUNCATED		(-1)
#define PATCH_CORRUPT_ERROR 	(-2)
#define IO_ERROR		(-3)
#define EOF_ERROR		(-4)
#define MEM_ERROR		(-5)
#define FORMAT_ERROR		(-6)
#define DATA_ERROR		(-7)

#define v0printf(expr...) fprintf(stderr, expr)

#ifdef DEV_VERSION
#include <assert.h>
#define eprintf(expr...)   { abort(); fprintf(stderr, expr); };
#define v1printf(expr...)  fprintf(stderr,expr)
#define v2printf(expr...)  { if(global_verbosity>0){fprintf(stderr,expr);};}
#define v3printf(expr...)  { if(global_verbosity>1){fprintf(stderr,expr);};}
#define v4printf(expr...)  { if(global_verbosity>2){fprintf(stderr,expr);};}
#else
#define assert(expr) ((void)0)
#define eprintf(expr...)   fprintf(stderr, expr)
#define v1printf(expr...)  { if(global_verbosity>0){fprintf(stderr,expr);};}
#define v2printf(expr...)  { if(global_verbosity>1){fprintf(stderr,expr);};}
#define v3printf(expr...)  { if(global_verbosity>2){fprintf(stderr,expr);};}
#define v4printf(expr...)  { if(global_verbosity>3){fprintf(stderr,expr);};}
#endif

#ifdef DEBUG_CFILE
#include <stdio.h>
#define dcprintf(fmt...) \
    fprintf(stderr, "%s: ",__FILE__);   \
    fprintf(stderr, fmt);
#else
#define dcprintf(expr...) ((void) 0);
#endif

#endif
