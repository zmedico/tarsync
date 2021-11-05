/*
  Copyright (C) 2005 Brian Harring
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

#ifndef _HEADER_TARSYNC_FS
#define _HEADER_TARSYNC_FS 1

int is_dir(const char *path, struct stat *s);
int is_reg(const char *path, struct stat *s);
int is_lnk(const char *path, struct stat *s);
int ensure_dirs(const char *path, mode_t mode, uid_t uid, gid_t gid);

#endif
