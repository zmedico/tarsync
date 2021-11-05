/*
  Copyright (C) 2003-2005 Brian Harring

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
#ifndef _HEADER_TAR
#define _HEADER_TAR 1

#include "defs.h"
#include "cfile.h"
#include <sys/stat.h>

/* the data structs were largely taken/modified/copied from gnutar's header file.
   I had to go there for the stupid gnu extension... */
#define TAR_NAME_LOC				0
#define TAR_MODE_LOC				100
#define TAR_UID_LOC				108
#define TAR_GID_LOC				116
#define TAR_SIZE_LOC				124
#define TAR_MTIME_LOC				136
#define TAR_CHKSUM_LOC				148
#define TAR_TYPEFLAG_LOC		156
#define TAR_LINKNAME_LOC		157
#define TAR_MAGIC_LOC				257
#define TAR_VERSION_LOC				263
#define TAR_UNAME_LOC				265
#define TAR_GNAME_LOC				297
#define TAR_DEVMAJOR_LOC		329
#define TAR_DEVMINOR_LOC		337
#define TAR_PREFIX_LOC				345

#define TAR_NAME_LEN				100
#define TAR_MODE_LEN				8
#define TAR_UID_LEN				8
#define TAR_GID_LEN				8
#define TAR_SIZE_LEN				12
#define TAR_MTIME_LEN				12
#define TAR_CHKSUM_LEN				8
#define TAR_TYPEFLAG_LEN		1
#define TAR_LINKNAME_LEN		100
#define TAR_MAGIC_LEN				6
#define TAR_VERSION_LEN				2
#define TAR_UNAME_LEN				32
#define TAR_GNAME_LEN				32
#define TAR_DEVMAJOR_LEN		8
#define TAR_DEVMINOR_LEN		8
#define TAR_PREFIX_LEN				155

#define TAR_EMPTY_ENTRY				0x1

#define REGTYPE			'0'		/* Regular file (preferred code).  */
#define AREGTYPE		'\0'	/* Regular file (alternate code).  */
#define LNKTYPE			'1'		/* Hard link.  */
#define SYMTYPE			'2'		/* Symbolic link (hard if not supported).  */
#define CHRTYPE			'3'		/* Character special.  */
#define BLKTYPE			'4'		/* Block special.  */
#define DIRTYPE			'5'		/* Directory.  */
#define FIFOTYPE		'6'		/* Named pipe.  */
#define CONTTYPE		'7'		/* Contiguous file */

#define TTAR_UNSUPPORTED_TYPE 255

typedef struct {
	off_u64			file_loc;
	off_u64			start;
	off_u64			size;
	unsigned int	fullname_len;
	char			*fullname;
	unsigned int	linkname_len;
	char			*linkname;
	time_t			mtime;
	uid_t			uid;
	gid_t			gid;
	mode_t			mode;
	unsigned char	type;
} tar_entry;

#define TTAR_ISDIR(x)	(DIRTYPE == (x)->type)
#define TTAR_ISREG(x)	(REGTYPE == (x)->type)

int check_str_chksum(const char *entry);
unsigned long octal_str2long(const char *string, unsigned int length);
signed int read_fh_to_tar_entry(cfile *src_fh, tar_entry **tar_entries, unsigned int *total_count, unsigned int strip_dirs);
int read_entry(cfile *src_cfh, off_u64 start, tar_entry *te);
int readh_cfh_to_tar_entries(cfile *src_cfh, tar_entry ***file, 
	unsigned long *total_count);
//struct tar_entry **read_fh_to_tar_entry(int src_fh, unsigned long *total_count);

int convert_lstat_type_tar_type(const char *path, struct stat *st);

#endif

