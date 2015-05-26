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

#include <stdlib.h>
#include <stdio.h>
#include <fnmatch.h>
#include "string-misc.h"
#include "excludes.h"
#include "defs.h"

int
build_exclude(fnm_exclude **ex_ptr, const char *pattern)
{
	char *p, *pat, *pat2;
	fnm_exclude *exclude;
	if(NULL == pattern || '\0' == *pattern)
		return -1;

	exclude = malloc(sizeof(fnm_exclude));
	if(NULL == exclude)
		return MEM_ERROR;

	exclude->anchored = 0;

	pat = strdup(pattern);
	pat2 = pat;
	if('/' == *pat) {
		exclude->anchored = START_ANCHORED;
		do	{
			pat++;
		} while ('\0' != *pat && '/' == *pat);
	} else if ('.' == *pat && '/' == *(pat+1)) {
		exclude->anchored = START_ANCHORED;
		pat += 2;
		while('\0' != *pat && '/' == *pat)
			pat++;
	} else {
		exclude->anchored = 0;
	}
	p = rindex(pat, '/');
	if(NULL != p && '\0' == *(p+1))
		exclude->anchored |= TAIL_ANCHORED;

	int skip = 0, dup = 0;

	// s:/{2,}:/:
	p = pat;
	while('\0' != *p) {
		if('/' == *p)
			dup++;
		else if(dup > 1) {
			skip += dup - 1;
			pat[p - pat - skip] = *p;
			dup = 0;
		}
		p++;
	}

	skip += dup;

	if(skip) {
		// trailing '/', and null the new collapsed path;
		pat[p - pat - skip -1] = '\0';
	}

	if('\0' == *pat) {
		// pattern was ^\.?/{1,}$
		free(exclude);
		return 1;
	}

	p = rindex(pat, '/');
	if (NULL == p) {
		// not bound to a dir exempting anchors
		exclude->dir = NULL;
		exclude->dir_depth = 0;
		exclude->file = strdup(pat);
	} else {
		exclude->dir = strndup(pat, p - pat + 1);
		exclude->file = strdup(p + 1);
		exclude->dir_depth = 1;
		while(p-- > pat) {
			if('/' == *p)
				exclude->dir_depth++;
		}
	}
	free(pat2);
	exclude->enabled = 1;
	*ex_ptr = exclude;
	return 0;
}


int
match_excludes(const char *dir, const char *file, fnm_exclude **excludes)
{
	char *p;
	int ret, x;
	unsigned int dir_depth;

	if (NULL == excludes)
		return 0;

	for(dir_depth = 0, p = (char *)dir; '\0' != *p; p++) {
		if('/' == *p) 
			dir_depth++;
	}
	ret = 0;
	for(x=0; ret == 0 &&  NULL != excludes[x]; x++) {
		if(! excludes[x]->enabled)
			continue;
		if((excludes[x]->anchored & START_ANCHORED) && excludes[x]->dir_depth != dir_depth)
			continue;
		if((excludes[x]->anchored & TAIL_ANCHORED) && strcmp(excludes[x]->file, file) == 0)
			ret = 1;

		ret = fnmatch(excludes[x]->file, file, FNM_PATHNAME);
		if(1 == ret)
			ret = 0;
		else if(0 == ret)
			ret = 1;
		else {
			// either an error, or a match.
			v0printf("got non zero ret %i on %s %s via %s %s\n", ret, dir, file, excludes[x]->dir, excludes[x]->file);
		}
	}
	return ret;
}


int
optimize_excludes(char *dpath, fnm_exclude **excludes)
{
	unsigned int dir_depth;
	char *d, *dir, *orig_dir, *file;
	int x;
	int ret;

	if(NULL == excludes)
		return 0;

	for(dir_depth = 0, file = NULL, d = (char *)dpath; '\0' != *d; d++) {
		if('/' == *d) {
			file = d;
			dir_depth++;
		}
	}
	dir = strdup(dpath);
	if(NULL != file) {
		dir[file - dpath] = '\0';
		file = file - dpath + dir + 1;
	} else {
		file = strlen(dir) + dir;
	}

	orig_dir = dir;

	for(x = 0; excludes[x] != NULL; x++) {
		dir = orig_dir;
		if((excludes[x]->anchored & START_ANCHORED) && excludes[x]->dir_depth != dir_depth) {
			excludes[x]->enabled = 0;
			continue;
		} else if(dir_depth < excludes[x]->dir_depth) {
			excludes[x]->enabled = 0;
			continue;
		}
		// reset dir to a point where it's applicable for this exclude
		if(dir_depth > excludes[x]->dir_depth) {
			ret = excludes[x]->dir_depth;
			d = dir + strlen(dir);
			for(; d > dir && ret >= 0; d--) {
				if('/' == *d) {
					ret--;
				}
			}
			dir = d;
			assert(*d == '/');
		}
		if(excludes[x]->dir)
			ret = fnmatch(excludes[x]->dir, dir, FNM_PATHNAME);
		else
			ret = 1;
		if(ret < 0) {
			v0printf("error from fnmatch, %i for pattern %s dir %s, orig_dir %s\n", ret, excludes[x]->dir, dir, orig_dir);
			free(orig_dir);
			return ret;
		} else if(ret == 0) {
			excludes[x]->enabled = 0;
		} else {
			excludes[x]->enabled = 1;
		}
	}
	free(orig_dir);
	return 0;
}			
