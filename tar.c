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
#include <stdio.h>
#include "string-misc.h"
#include <stdlib.h>
#include <unistd.h>
#include "defs.h"
#include "tar.h"
#include "names.h"

int check_str_chksum(const char *block)
{
	/* the chksum is 8 bytes and lives at byte 148.
	  for where the chksum exists in the string, you treat each byte as ' '*/
	unsigned long chksum=0;
	unsigned int x;
	for(x=0; x< 148; x++) {
		chksum += (unsigned char)block[x];
	}
	chksum+=' '* 8;
	for(x=156; x< 512; x++)
		chksum += (unsigned char)block[x];
	return (chksum==octal_str2long((char *)(block + TAR_CHKSUM_LOC), TAR_CHKSUM_LEN));
}

/* possibly this could be done different, what of endptr of strtol?
   Frankly I worry about strtol trying to go too far and causing a segfault, due to tar fields not always having trailing \0 */
inline unsigned long octal_str2long(const char *string, unsigned int length)
{
	if(string[length]) {
		char *ptr = strndup(string, length);
		unsigned long x;
		x = strtol((char *)ptr, NULL, 8);
		free(ptr);
		return(x);
	} else 
		return strtol(string, NULL, 8);
}

int
read_fh_to_tar_entry(cfile *src_cfh, tar_entry **tar_entries, 
	unsigned int *total_count, unsigned int strip_dir) 
{
	tar_entry *entries;
	unsigned long array_size=10000;
	unsigned long count =0;
	signed int err = 0;
	unsigned int strip_count;
	char *p;
	if((entries = (tar_entry *)calloc(array_size,sizeof(tar_entry)))==NULL){
		return MEM_ERROR;
	}
	while(!err) {		
		if(count==array_size) {
			// out of room, resize 
			if ((entries = (tar_entry *)realloc(entries
					,(array_size += 10000)*sizeof(tar_entry)))==NULL){
				v0printf("unable to allocate %lu tar entries (needed), bailing\n", array_size);
				return MEM_ERROR;
			}
		}
		err = read_entry(src_cfh, (count == 0 ? 0 : entries[count -1].file_loc + 
			entries[count - 1].size + (entries[count -1].size % 512 ? 
			(512 - (entries[count -1].size % 512)) : 0)), entries + count);

		if(0 == err) {
			if(strip_dir) {
				p = index(entries[count].fullname, '/');
				for(strip_count = 1; strip_count < strip_dir && NULL != p; strip_count++)
					p = index(p + 1, '/');
				if(NULL == p) {
					// couldn't do required strip.  puke it.
					v0printf("failed attempting to strip %i dirs from entry '%s'\n", strip_dir, entries[count].fullname);
					return 1;
				}
				entries[count].fullname = p + 1;
				entries[count].fullname_len = strlen(entries[count].fullname);
			}
			if(entries[count].fullname_len) {
				if('/' == entries[count].fullname[entries[count].fullname_len - 1])
					entries[count].fullname[entries[count].fullname_len - 1] = '\0';
			}

		}
//		err = read_entry(src_cfh, (count == 0 ? 0 : entries[count - 1].end), &entries[count]);
		count++;
	}
	if(err != TAR_EMPTY_ENTRY) {
		// not deallocing the alloc'd mem for each entry (fullname), fix this prior to going lib
			free(entries);
			return err;
	}
	count--;			
	if(!count)
			return EOF_ERROR;
	
	if ((entries=(tar_entry *)realloc(entries, count * sizeof(tar_entry)))==NULL){
		v0printf("error reallocing tar array (specifically releasing, odd), bailing\n");
		return MEM_ERROR;
	}
	*tar_entries = entries;
	*total_count = count;
	return 0;
}

int
read_entry(cfile *src_cfh, off_u64 start, tar_entry *entry)
{
	unsigned char block[512];
	unsigned int read_bytes;
	unsigned int name_len, prefix_len;

	if(start != cseek(src_cfh, start, CSEEK_FSTART)) {
			return IO_ERROR;
	}
	if((read_bytes=cread(src_cfh, block, 512))!=512) {
			return IO_ERROR;
	}
	entry->start = start;
	entry->file_loc = 512 + start;

	if(strnlen(block, 512)==0)  {
		return TAR_EMPTY_ENTRY;
	}

	if (! check_str_chksum((const char *)block)) {
		v0printf("checksum failed on a tarfile, bailing\n");
		return IO_ERROR;
	}

	if('L'==block[TAR_TYPEFLAG_LOC]) {
		v2printf("handling longlink at %llu eg(%llu)\n", (act_off_u64)start, (act_off_u64)(start * 512));

		name_len = octal_str2long(block + TAR_SIZE_LOC, TAR_SIZE_LEN);

		if((read_bytes=cread(src_cfh, block, 512))!=512) {
			v0printf("unexpected EOF on tarfile, bailing\n");
			return EOF_ERROR;
		}
		entry->file_loc += 1024;
		if((entry->fullname = 
			(unsigned char *)malloc(name_len + 1))==NULL){
			v0printf("unable to allocate memory for name_len, bailing\n");
			return EOF_ERROR;
		}

		memcpy(entry->fullname, block, name_len);
		if((read_bytes=cread(src_cfh, block, 512))!=512){
			v0printf("unable to allocate memory for fullname, bailing\n");
			return EOF_ERROR;
		}

		if(! check_str_chksum((const char *)block)) {
			v0printf("tar checksum failed for tar entry at %llu, bailing\n", (act_off_u64)start);
			// IO_ERROR? please.  add data_error.
			return IO_ERROR;
		}

		entry->fullname[name_len] = '\0';
		entry->fullname_len = name_len;

	} else {

		name_len = strnlen(block + TAR_NAME_LOC, TAR_NAME_LEN);
		prefix_len = strnlen(block + TAR_PREFIX_LOC, TAR_PREFIX_LEN);

		// check if space will be needed for the slash
		prefix_len += (prefix_len==0 ? 0 : 1);
		if((entry->fullname = 
			(unsigned char *)malloc(name_len + prefix_len + 1))==NULL){
			v0printf("unable to allocate needed memory, bailing\n");
			return MEM_ERROR;
		}
		if(prefix_len) {
			memcpy(entry->fullname, block + TAR_PREFIX_LOC, prefix_len -1);
			entry->fullname[prefix_len] = '/';
			memcpy(entry->fullname + prefix_len, block + TAR_NAME_LOC, name_len);
			entry->fullname[prefix_len + name_len ] = '\0';
			entry->fullname_len = prefix_len + name_len;
		} else {
			memcpy(entry->fullname, block + TAR_NAME_LOC, name_len);
			entry->fullname[name_len] = '\0';
			entry->fullname_len = name_len;
		}
//		entry->end += octal_str2long(block + TAR_SIZE_LOC, TAR_SIZE_LEN);
//		entry->size = octal_str2long(block + TAR_SIZE_LOC, TAR_SIZE_LEN);

	}

	entry->size = octal_str2long(block + TAR_SIZE_LOC, TAR_SIZE_LEN);
	entry->mtime = octal_str2long(block + TAR_MTIME_LOC, TAR_MTIME_LEN);
	entry->mode = octal_str2long(block + TAR_MODE_LOC, TAR_MODE_LEN);

	if(block[TAR_TYPEFLAG_LOC] < '8' && block[TAR_TYPEFLAG_LOC] >= '0') {
		switch(block[TAR_TYPEFLAG_LOC]) {
		case CONTTYPE:
			v0printf("warning: conttype, probably won't handle it right.\n");
		case REGTYPE:
		case AREGTYPE:
			entry->type = REGTYPE;	break;
		case SYMTYPE:
			v0printf("symlinks not supported\n");
			entry->type = TTAR_UNSUPPORTED_TYPE; break;
		case LNKTYPE:
			v0printf("hardlinks not supported!\n");
			entry->type = TTAR_UNSUPPORTED_TYPE; break;
		case CHRTYPE:
			v0printf("char type, unsupported\n");
			entry->type = TTAR_UNSUPPORTED_TYPE; break;
		case BLKTYPE:
			v0printf("block type, unsupported\n");
			entry->type = TTAR_UNSUPPORTED_TYPE; break;
		case DIRTYPE:
			entry->type = DIRTYPE;	break;
		case FIFOTYPE:
			v0printf("fifo type, unsupported\n");
			entry->type = TTAR_UNSUPPORTED_TYPE; break;
		}
	} else {
		v0printf("encountered type '%c', probably going to screw it up.\n", block[TAR_TYPEFLAG_LOC]);
		entry->type = TTAR_UNSUPPORTED_TYPE;
	}

	if(get_gid(block + TAR_GNAME_LOC, &entry->gid))
		entry->gid = octal_str2long(block + TAR_GID_LOC, TAR_GID_LOC);

	if(get_uid(block + TAR_UNAME_LOC, &entry->uid))
		entry->uid = octal_str2long(block + TAR_UID_LOC, TAR_UID_LOC);

//	if(entry->end % 512)
//		entry->end += 512 - (entry->end % 512);
	return 0;
}

int
convert_lstat_type_tar_type(const char *path, struct stat *st)
{
	if(lstat(path, st) != 0)
		return -1;
	// we don't like hardlinks currently
	if(S_ISREG(st->st_mode)) {
		if(st->st_nlink == 1)
			return REGTYPE;
	} else if(S_ISDIR(st->st_mode))
			return DIRTYPE;

	return TTAR_UNSUPPORTED_TYPE;
}
