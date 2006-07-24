/*
  Copyright (C) 2005 Brian Harring

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
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int
is_dir(const char *path, struct stat *s)
{
	s->st_mode = 0;
	if(lstat(path, s) != 0)
		return -1;
	return S_ISDIR(s->st_mode) ? 0 : 1;
}

int
is_reg(const char *path, struct stat *s)
{
	s->st_mode = 0;
	if(lstat(path, s) != 0)
		return -1;
	return S_ISREG(s->st_mode) ? 0: 1;
}

int
is_lnk(const char *path, struct stat *s)
{
	s->st_mode = 0;
	if(lstat(path, s) != 0)
		return -1;
	return S_ISLNK(s->st_mode) ? 0: 1;
}

int
ensure_dirs(const char *path, mode_t mode, uid_t uid, gid_t gid)
{
	struct stat s;
	char *p2 = NULL, *p = NULL;
	if(is_dir(path, &s) != 0) {
		if(ENOENT == errno) {
			// missing dir component... yay.
			// try the "rightmost is missing" first.

			if(ENOENT == errno && mkdir(path, 0777) == 0)
				goto verify_ensure_dir_perms;
			
			p2 = strdup(path);

			if('/' == *p2)		p = index(p2 + 1, '/');
			else 				p=index(p2, '/');

			// loop creating dirs as normal user.
			while(NULL != p) {
				*p = '\0';
				if(lstat(p2, &s) != 0 && ENOENT != errno) {
					return -1;
				} else {
					if(mkdir(p2, 0777) != 0)
						return -1;
				}
				*p = '/';
				p = index(p + 1, '/');
			}
		} else if(ENOTDIR == errno) {
			p2 = strdup(path);
			if('/' == *p2)		p = index(p2 + 1, '/');
			else 				p=index(p2, '/');

			// wipe file where it's found, and create dirs as normal user.
			while(NULL != p) {
				*p = '\0';
				if(lstat(p2, &s) != 0 && ENOENT != errno) {
					return -1;
				} else {
					if(unlink(p2) != 0)
						return -1;
					if(mkdir(p2, 0777) != 0)
						return -1;
				}
				*p = '/';
				p = index(p + 1, '/');
			}
		} else
			return -1;
	} else {
		if( (( s.st_mode & 0777 ) != ( mode & 0777 )) && (chmod(path, mode) != 0) )
			return -1;
		if( (( s.st_uid != uid ) || ( s.st_gid != gid )) && (lchown(path, uid, gid) != 0) )
			return -1;
		return 0;
	}
			
	if(p2)
		free(p2);

	verify_ensure_dir_perms:

	if(chmod(path, mode) != 0)
		return -1;
	if(lchown(path, uid, gid) != 0)
		return -1;
	return 0;
}		

