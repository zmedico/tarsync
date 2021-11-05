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

#include <sys/types.h>
#include <grp.h>
#include <pwd.h>
#include "string-misc.h"

#ifndef NULL
#define NULL 0
#endif

//limited due to main use of this, for tar.c
//remove this restriction if reusing this

#define name_size 64
static char cached_uname[name_size];
static char cached_gname[name_size];
static gid_t cached_gid;
static uid_t cached_uid;

int
get_gid(const char *gname, gid_t *gid)
{
    struct group *ggid = NULL;
	if(strncmp(gname, (const char *)cached_gname, name_size) != 0) {
		ggid = getgrnam(gname);
		if(ggid) {
			strncpy(cached_gname, (const char *)ggid->gr_name, name_size);
			cached_gid = ggid->gr_gid;
		} else {
			return 1;
		}
	}
	*gid = cached_gid;
	return 0;
}

int
get_uid(const char *uname, uid_t *uid)
{
    struct passwd *uuid = NULL;
	if(strncmp(uname, cached_uname, name_size) != 0) {
		uuid = getpwnam(uname);
		if(uuid) {
			strncpy(cached_uname, uname, name_size);
			cached_uid = uuid->pw_uid;
		} else {
			return 1;
		}
	}
	*uid = cached_uid;
	return 0;
}

