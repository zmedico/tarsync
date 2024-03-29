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

#ifndef _EXCLUDES_HEADER
#define _EXCLUDES_HEADER 1

typedef struct {
	char *dir;
	unsigned int dir_depth;
	char *file;
	unsigned char anchored;
	unsigned char enabled;
} fnm_exclude;

#define START_ANCHORED	0x1
#define TAIL_ANCHORED	0x2

int match_excludes(const char *dir, const char *file, fnm_exclude **excludes);
int build_exclude(fnm_exclude **excludes, const char *pattern);
int optimize_excludes(char *dpath, fnm_exclude **excludes);

#endif

