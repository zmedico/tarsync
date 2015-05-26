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
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <utime.h>
#include "options.h"
#include "tar.h"
#include <cfile.h>
#include "fs.h"
#include "names.h"
#include "excludes.h"

unsigned int global_verbosity=0;

int cmp_tar_entries_name(const void *, const void *);
int cmp_tar_entries_loc(const void *, const void *);
unsigned int cleanse_duds(tar_entry **ttar, unsigned int ttar_count);
int recursively_delete_dir(const char *path);
int remove_node(const char *path, struct stat *st);
int ensure_files_layout(const tar_entry **ttar, const unsigned int ttar_count, tar_entry ***missing, 
	unsigned int *missing_count, tar_entry ***existing, unsigned int *existing_count,
	fnm_exclude **excludes);
int check_existing_node(const struct dirent *de, const tar_entry *t, struct stat *st);
int enforce_owner(const char *path, const tar_entry *t, struct stat *st);
int copy_whole_file(cfile *tar_cfh, const tar_entry *ttent);

static int check_mtime = 1;
static time_t start_time = 0;

static struct option long_opt[] = {
	STD_LONG_OPTIONS,
	FORMAT_LONG_OPTION("strip-dirs",'s'),
	FORMAT_LONG_OPTION("override-user",'o'),
	FORMAT_LONG_OPTION("override-group",'g'),
	FORMAT_LONG_OPTION("exclude", 'e'),
	FORMAT_LONG_OPTION("preserve", 'p'),
	END_LONG_OPTS
};
static char short_opts[] = STD_SHORT_OPTIONS "s:o:g:e:p";
static struct usage_options help_opts[] = {
	STD_HELP_OPTIONS,
	FORMAT_HELP_OPTION("strip-dirs", 's', "specify the number of directories to strip from the tarball archive during recreation"),
	FORMAT_HELP_OPTION("overide-owner", 'o', "Ensure files has this owner, instead of what the tarball states"),
	FORMAT_HELP_OPTION("overide-group", 'g', "Ensure files have this username, instead of what the tarball states"),
	FORMAT_HELP_OPTION("exclude-fnmatch",'e',"Specify a glob pattern for excluding files."),
	FORMAT_HELP_OPTION("preserve", 'p', "Enforce tarball permissions, rather then users (modified by -o and -g)."),
	USAGE_FLUFF("tarsync expects two args, a tarball (can be bzip2 or gzip compressed), and a name for the directory to 'sync'\n"
	"up to the tarballs contents\n"
	"Example usage: tarsync --strip-dir 1 portage-20050511.tar.bz2 /usr/portage"),
	END_HELP_OPTS
};


#define DUMP_USAGE(exit_code)	\
print_usage("tarsync", "[options] tarball directory target", help_opts, exit_code)


int
main(int argc, char **argv)
{
	char *tarball_name = NULL;
	char *dir_name = NULL;
	fnm_exclude **excludes = NULL;
	cfile tar_cfh;
	tar_entry *ttar = NULL;
	tar_entry **ttar_sorted = NULL;
	unsigned int ttar_count = 0;
	unsigned int ttar_sorted_count = 0;
	struct stat s;
	off_u64 bytes_written = 0;
	int x, ret;
	unsigned int strip = 0;
	int optr, preserve_tar_privs = 0;
	int override_uid = 0, override_gid = 0;
	unsigned int excludes_count = 0, excludes_size = 0;
	uid_t uid; 	gid_t gid;
	start_time = time(NULL);

	// opts parsing. blech.
	while((optr = getopt_long(argc, argv, short_opts, long_opt, NULL)) != -1) {
		switch(optr) {
			case OVERSION:
				print_version("tarsync"); exit(0);
			case OUSAGE:
			case OHELP:
				DUMP_USAGE(0);
			case OVERBOSE:
				global_verbosity++;
				break;
			case 'o':
				if(override_uid)
					v0printf("a uid override has already been specified.\n");
				else if(get_uid(optarg, &uid) != 0)
					v0printf("failed looking up uid for %s\n", optarg);
				else
					override_uid = 1;
				if(0 == override_uid)
					DUMP_USAGE(EXIT_USAGE);
				break;
			case 'g':
				if(override_gid)
					v0printf("a gid override has already been specified.\n");
				else if(get_gid(optarg, &gid) != 0)
					v0printf("failed looking up gid for %s\n", optarg);
				else
					override_gid = 1;
				if(0 == override_gid)
					DUMP_USAGE(EXIT_USAGE);
				break;
			case 'p':
				preserve_tar_privs = 1;
				break;
			case 's':
				if(0 != strip) {
					v0printf("a strip level has already been specified.  only one can be specified!\n");
					DUMP_USAGE(EXIT_USAGE);
				}
				strip = atoi(optarg);
				if(strip <= 0) {
					v0printf("--strip-dir arg must be numeric, and greater then zero!\n");
					DUMP_USAGE(EXIT_USAGE);
				}
				break;
			case 'e':
				if(excludes_count + 1 >= excludes_size) {
					excludes = realloc(excludes, sizeof(fnm_exclude *) * (excludes_size + 16));
					if(NULL == excludes) {
						v0printf("failed to alloc required memory for excludes\n");
						exit(-1);
					}
					excludes_size += 16;
				}
				ret = build_exclude(&excludes[excludes_count], optarg);
				if(ret > 1) {
					v0printf("invalid pattern for %s!\n", optarg);
					DUMP_USAGE(EXIT_USAGE);
				} else if(ret < 0) {
					v0printf("failed adding exclude '%s', mem?\n", optarg);
					exit(-1);
				}
				excludes_count++;
				break;
			default:
				v0printf("invalid arg- %s\n", argv[optind]);
				DUMP_USAGE(EXIT_USAGE);
		}
	}

	tarball_name = get_next_arg(argc, argv);
	dir_name = get_next_arg(argc, argv);

	if(tarball_name == NULL) {
		v0printf("need to specify the tarball, and target directory!\n");
		DUMP_USAGE(EXIT_USAGE);
	}
	if(copen(&tar_cfh, tarball_name, AUTODETECT_COMPRESSOR, CFILE_RONLY)) {
		v0printf("failed opening %s", tarball_name);
		exit(1);
	}
	if(NULL == dir_name) {
		v0printf("need to specify the tarball, and target directory!\n");
		DUMP_USAGE(EXIT_USAGE);
	}

	// finish excludes setup.
	if(NULL != excludes) {
		for(x = excludes_count; x < excludes_size; x++) 
			excludes[x] = NULL;
	}

	if((ttar = (tar_entry *)malloc(sizeof(tar_entry) * 1024)) == NULL) {
		v0printf("mem error allocing mr tar_entry.  die\n");
		exit(2);
	}
	ttar_count = 1024;
	v0printf("scanning tarball...\n");
	if(read_fh_to_tar_entry(&tar_cfh, &ttar, &ttar_count, strip)) {
		v0printf("error reading %s\n", tarball_name);
		exit(3);
	}

	ttar_sorted = (tar_entry **)malloc(sizeof(tar_entry *)*ttar_count);
	for(x=0; x < ttar_count; x++) 
		ttar_sorted[x] = ttar + x;

	qsort(ttar_sorted, ttar_count, sizeof(tar_entry *), cmp_tar_entries_name);

	ttar_sorted_count = cleanse_duds(ttar_sorted, ttar_count);
	ttar_sorted = realloc(ttar_sorted, sizeof(tar_entry *) * ttar_sorted_count);

	if(! preserve_tar_privs) {
		if(!override_uid) {
			uid = getuid();
			override_uid = 1;
		}
		if(!override_gid) {
			override_gid = 1;
			gid = getgid();
		}
	}
	for(x=0; x < ttar_sorted_count; x++) {
		if(override_uid)
			ttar_sorted[x]->uid = uid;
		if(override_gid)
			ttar_sorted[x]->gid = gid;
	}
	s.st_mode = 0;
	
	// ensure target dir, and chdir.  any fail, bail.
	if( !(is_dir(dir_name, &s) == 0 || ensure_dirs(dir_name, 0777, -1, -1) == 0 ) || chdir(dir_name) != 0 ) {
		v0printf("something is screwy with target dir %s\n", dir_name);
		exit(4);
	}

	unsigned int missing_count, existing_count;
	tar_entry **missing, **existing;
	missing = NULL;
	existing = NULL;

	v0printf("scanning existing target directory...\n");
	if(ensure_files_layout((const tar_entry **)ttar_sorted, ttar_sorted_count, &missing, &missing_count,
		&existing, &existing_count, excludes) != 0) {
		v0printf("failed enforcing file layout, bailing\n");
		exit(6);
	}
	v0printf("%u to update\n", missing_count);
	// by this time, we've got our directory structure in place, and removed all crap files.
	// haven't done anything about ensuring a file is what it's supposed to be, however.

	
	// only sort if the tarball is compressed.
	if(NO_COMPRESSOR != tar_cfh.compressor_type) {
		qsort(missing, missing_count, sizeof(tar_entry *), cmp_tar_entries_loc);
	}
	// no need to seek.  cfile handles resetting streams as needed
	
	for(x=0; x < missing_count; x++) {
		if(copy_whole_file(&tar_cfh, missing[x]) != 0) {
			v0printf("failed transfering file %s\n", missing[x]->fullname);
			exit(9);
		}
		bytes_written += missing[x]->size;
	}
	v0printf("%u files written, %u entires verified, %llu bytes written\n", missing_count, existing_count, (act_off_u64)bytes_written);

	while(strip--)
		chdir("..");
	exit(0);
}

unsigned int 
cleanse_duds(tar_entry **ttar, unsigned int ttar_count)
{
	unsigned int dups = 0;
	unsigned int x;
	if(ttar_count > 0) {
		if(TTAR_UNSUPPORTED_TYPE == ttar[0]->type || 0 == ttar[0]->fullname_len) 
			dups++;
	}
	for(x = 1; x < ttar_count; x++) {
		if((ttar[x]->fullname_len == ttar[x - 1]->fullname_len && 
			strcmp(ttar[x]->fullname, ttar[x - 1]->fullname) == 0) || 
			(TTAR_UNSUPPORTED_TYPE == ttar[x]->type) ||
			(0 == ttar[x]->fullname_len)) {
			// dud.
			dups++;
		} else {
			if(dups)
				ttar[x - dups] = ttar[x];
		}			
	}
	if(ttar_count > 0 && dups) 
		ttar[ttar_count -1 - dups] = ttar[ttar_count - 1];
	v1printf("cleansed %i dups out of %i\n", dups, ttar_count);
	return ttar_count - dups;
}


#define TTENT_PTR(z) (*((tar_entry **)(z)))

// sort by file loc.
int
cmp_tar_entries_loc(const void *te1, const void *te2)
{
	if(TTENT_PTR(te1)->file_loc > TTENT_PTR(te2)->file_loc)
		return 1;
	else if(TTENT_PTR(te1)->file_loc < TTENT_PTR(te2)->file_loc)
		return -1;
	return 0;
}

// this is... odd.  it basically sorts it into bredth/depth alphasort for dirs.

int
cmp_tar_entries_name(const void *te1, const void *te2)
{
	int ret, cur, next;
	char *p1, *p2;

	cur = next = 0;

	do {
		p1 = index(TTENT_PTR(te1)->fullname + cur, '/');
		p2 = index(TTENT_PTR(te2)->fullname + cur, '/');
		if(NULL == p1 && NULL != p2)
			return 	-1;
		else if(NULL != p1 && NULL == p2)
			return 1;
		else if((NULL == p1 && NULL == p2)) {
			ret = strcmp(TTENT_PTR(te1)->fullname + cur, TTENT_PTR(te2)->fullname + cur);
			if(ret != 0)
				return ret;
			if(TTENT_PTR(te1)->file_loc > TTENT_PTR(te2)->file_loc)
				return 1;
			return -1;
		} else if(p1 - TTENT_PTR(te1)->fullname != p2 - TTENT_PTR(te2)->fullname) {
			return strcmp(TTENT_PTR(te1)->fullname + cur, TTENT_PTR(te2)->fullname + cur);
		}

		next = p1 - TTENT_PTR(te1)->fullname;

		ret = strncmp(TTENT_PTR(te1)->fullname + cur, TTENT_PTR(te2)->fullname + cur, next - cur);
		if(ret != 0)
			return ret;
		cur = next + 1;
	} while (cur != 0);

	// this shouldn't be reachable.
	abort();
}


#define TTENT_check_entry_size(ents, count, size)						\
if((count) >= (size)) {													\
	(ents) = realloc((ents), sizeof(tar_entry *) * ((size) + 4096));	\
	if((ents) == NULL){													\
		v0printf("bailing due to failled realloc for ents\n");			\
		return -1;														\
	}																	\
	(size) += 4096;														\
}

#define TTENT_add_missing(node)										\
TTENT_check_entry_size(missing, *missing_count, missing_size);		\
if(TTAR_ISDIR(node)) {												\
	v1printf("creating dir '%s'\n", (node)->fullname);				\
	if(ensure_dirs((node)->fullname + curdir_len, (node)->mode, (node)->uid, (node)->gid) != 0) { \
		v0printf("failed ensuring dir %s\n", (node)->fullname);		\
		return -1;													\
	}																\
	TTENT_add_existing(node)										\
} else {															\
	missing[*missing_count] = (node);								\
	*(missing_count) += 1;											\
}

#define TTENT_add_existing(node)									\
TTENT_check_entry_size(existing, *existing_count, existing_size);	\
existing[*existing_count] = (node);									\
*(existing_count)+=1;

#define check_match_ret(dir, file, ret)									\
if((ret) < 0)	{														\
	v0printf("exclusion matching failure, dir %s ent %s error %i\n", 	\
		(dir), (file), (ret));											\
	return ret;															\
}

/* 
  kill anything that dares to wander into the basedir being updated.
  fools with chdir.  wind up where ya started if things go right.

  Must be passed a sorted tar_entry list.  If it ain't, you get to pick up the pieces. 
*/

int
ensure_files_layout(const tar_entry **ttar, const unsigned int ttar_count, tar_entry ***missing_ptr, unsigned int *missing_count,
tar_entry ***existing_ptr, unsigned int *existing_count, fnm_exclude **excludes)
{
	unsigned int x, dir_index;
	int dir_count;
	char *curdir_name = NULL;
	unsigned int curdir_len = 0;
	struct dirent  **dir_ent = NULL;
	char *p;
	tar_entry **missing = NULL, **existing = NULL;
	unsigned int dirs_deep = 0;
	unsigned int missing_size, existing_size;
	struct stat st_cached;
	int ret;
	*missing_count = 0;
	*existing_count = 0;
	existing_size = missing_size = 4096;

	if(0 == ttar_count)
		return 0;

	if((existing = malloc(sizeof(tar_entry *) * 4096)) == NULL) {
		v0printf("allocing failed\n");
		return -1;
	}
	if((missing = malloc(sizeof(tar_entry *) * 4096)) == NULL) {
		v0printf("allocing failed\n");
		return -1;
	}

	x = 0; dir_index = 0; dir_count = 0;
	while(x < ttar_count) {
		if(NULL == dir_ent) {
			if(curdir_name)
				free(curdir_name);
	
			while(dirs_deep--)
				chdir("..");
			
			p = rindex(ttar[x]->fullname, '/');
			dirs_deep=0;
			if(NULL == p) {
				p = strdup(".");
				curdir_name = strdup("");
			} else {
				char *d;
				p = strndup(ttar[x]->fullname, p + 1 - ttar[x]->fullname);
				curdir_name = strdup(p);
				d = p;
				while((d = index(d, '/')) != NULL) {
					dirs_deep++;
					d++;
				}
			}
			
			if(NULL == p) {
				v0printf("1 malloc failure on %s\n", ttar[x]->fullname);
				return -1;
			}
			dir_index = 0;
			curdir_len = strlen(curdir_name);
			ret = optimize_excludes(curdir_name, excludes);
			if(ret) {
				v0printf("non zero return from optimize_excludes, %i\n", ret);
				return ret;
			}
			ret = match_excludes(curdir_name, ttar[x]->fullname + curdir_len, excludes);
			check_match_ret(curdir_name, ttar[x]->fullname + curdir_len, ret);
			if(ret && TTAR_ISDIR(ttar[x])) {
				// well. the dir was matched.
				v0printf("not digging into dir %s (via %s) or it's children due to exclussion\n", p, ttar[x]->fullname);
				while(x < ttar_count && ttar[x]->fullname_len > curdir_len && 
					0 == strncmp(ttar[x]->fullname, curdir_name, curdir_len)) {
					x++;
				}
				dirs_deep = 0;
				continue;
			}

			if((dir_count = scandir(p, &dir_ent, 0, alphasort)) < 0) {
				v0printf("2 malloc failure on %s %s, %s\n", ttar[x]->fullname, p, get_current_dir_name());
				return -1;
			}
			if(dirs_deep >0 && chdir(curdir_name)!=0) {
				v0printf("failed chdir'ing to %s\n", curdir_name);
				return -1;
			}
			free(p);
		}
	
		// dual list processing.  loop over tar list, and dir list, nuking files as needed.
		while(dir_count > dir_index) {
			if(strcmp(dir_ent[dir_index]->d_name, "..") == 0 || strcmp(dir_ent[dir_index]->d_name, ".") == 0) {
				dir_index++;
				continue;
			}
			if(x > ttar_count || curdir_len > ttar[x]->fullname_len || 
				strncmp(ttar[x]->fullname, curdir_name, curdir_len) != 0 ||
				(p = index(ttar[x]->fullname + curdir_len + 1, '/')) != NULL) {
	
				// so ttar has moved onto another dir.  meaning it's time to wax any remaining dirents
				
				while(dir_count > dir_index) {
					ret = match_excludes(curdir_name, dir_ent[dir_index]->d_name, excludes);
					check_match_ret(curdir_name, dir_ent[dir_index]->d_name, ret);
					if(!ret) {
						v1printf("removing node '%s' '%s'\n", curdir_name, dir_ent[dir_index]->d_name);
						if(remove_node(dir_ent[dir_index]->d_name, NULL) != 0) {
							v0printf("failed removing '%s' in dir '%s'\n", dir_ent[dir_index]->d_name, curdir_name);
							return -1;
						}
					}
					free(dir_ent[dir_index]);
					dir_index++;
				}
				continue;
			}

			// still processing dir_ents and ttar.
			ret = strcmp(dir_ent[dir_index]->d_name, ttar[x]->fullname + curdir_len);
			if(ret < 0) {
				ret = match_excludes(curdir_name, dir_ent[dir_index]->d_name, excludes);
				check_match_ret(curdir_name, dir_ent[dir_index]->d_name, ret);
				if(! ret) {
					v1printf("removing node '%s' '%s'\n", curdir_name, dir_ent[dir_index]->d_name);
					if(remove_node(dir_ent[dir_index]->d_name, NULL) != 0)
						return -1;
				}
				free(dir_ent[dir_index]);
				dir_index++;
			} else if(ret == 0) {
				ret = match_excludes(curdir_name, dir_ent[dir_index]->d_name, excludes);
				check_match_ret(curdir_name, dir_ent[dir_index]->d_name, ret);
				if(! ret) {
					ret = check_existing_node(dir_ent[dir_index], ttar[x], &st_cached);
					if(ret < 0) {
						v0printf("failed checking entry %s\n", ttar[x]->fullname);
						return -1;
					}
					if(ret > 0) {
						if(ret < 3) {
							v1printf("removing node '%s' '%s'\n", curdir_name, dir_ent[dir_index]->d_name);
							if(remove_node(dir_ent[dir_index]->d_name, &st_cached) != 0) {
								v0printf("failed removing node %s\n", dir_ent[dir_index]->d_name);
								return -1;
							}
						}
						TTENT_add_missing((tar_entry *)ttar[x]);
					} else {
						if(enforce_owner(dir_ent[dir_index]->d_name, ttar[x], &st_cached) != 0) {
							v0printf("failed enforcing perms/owner for %s\n", ttar[x]->fullname);
							return -1;
						}
						TTENT_add_existing((tar_entry *)ttar[x]);
					}
				}
				free(dir_ent[dir_index]);
				dir_index++;
				x++;
			} else {
				ret = match_excludes(curdir_name, ttar[x]->fullname + curdir_len, excludes);
				check_match_ret(curdir_name, ttar[x]->fullname + curdir_len, ret);
				if(!ret) {
					TTENT_add_missing((tar_entry *)ttar[x]);
				}
				x++;
			}
		}
	
		// if we've made it here, then no more dir_ents left.
		free(dir_ent);
		dir_ent = NULL;
	
		// make sure we've moved to the new curdir in tar_ents
		while(x < ttar_count && ttar[x]->fullname_len > curdir_len && 
			0 == strncmp(ttar[x]->fullname, curdir_name, curdir_len) &&
			index(ttar[x]->fullname + curdir_len, '/') == NULL) {
			ret = match_excludes(curdir_name, ttar[x]->fullname + curdir_len, excludes);
			check_match_ret(curdir_name, ttar[x]->fullname + curdir_len, ret);
			if(!ret) {
				TTENT_add_missing((tar_entry *)ttar[x]);
			}
			x++;
		}
	}
	// yay.
	if(*missing_count == 0) {
		free(missing);
		missing_ptr = NULL;
	} else {
		*missing_ptr = realloc(missing, sizeof(tar_entry *) * (*missing_count));
	}
	if(*existing_count == 0) {
		free(existing);
		existing_ptr = NULL;
	} else {
		*existing_ptr = realloc(existing, sizeof(tar_entry *) * (*existing_count));
	}	

	// rewind dir.
	p = index(curdir_name, '/');
	while(NULL != p) {
		chdir("..");
		p = index(p + 1, '/');
	}
	return 0;
}

int
remove_node(const char *path, struct stat *st)
{
	struct stat st2;
	if(NULL == st) {
		if(lstat(path, &st2) != 0) {
			v0printf("odd. lstat failed on %s\n", path);
			return -1;
		}
		st = &st2;
	}
	if(S_ISDIR(st->st_mode)) {
		// damn.  recursively kill the dir.
		if(recursively_delete_dir(path) != 0) {
			v0printf("failed recursively deleting dir %s\n", path);
			return -1;
		}
	} else {
		if(unlink(path) != 0) {
			v0printf("failed removing %s\n", path);
			return -1;
		}
	}
	return 0;
}

int
recursively_delete_dir(const char *path)
{
	DIR *curdir;
	struct dirent *de;
	int ret = 0;

	curdir=opendir(path);

	if(NULL == curdir)
		return -1;
	if(chdir(path) != 0)
		ret = -1;
	while((de = readdir(curdir)) != NULL && ret == 0) {
		if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name,"..") == 0)
			continue;
		ret = remove_node(de->d_name, NULL);
	}
	closedir(curdir);
	char *p, *d;
	d=strdup(path);
	p = rindex(d, '/');
	if(NULL == p)
		p = d;

	do {
		if(chdir("..") != 0)
			return -1;
		if(ret == 0)
			ret = rmdir(p);
		*p = '\0';
		p = rindex(d, '/');
		if(p != NULL)
			p++;
	} while (NULL != p);
	free(d);
	return ret;
}

int
check_existing_node(const struct dirent *de, const tar_entry *t, struct stat *st)
{
	int type;
	type = convert_lstat_type_tar_type(de->d_name, st);
	if(type < 0)
		return -1;
	if(TTAR_UNSUPPORTED_TYPE == type)
		return 1;
	if(type != t->type)
		return 2;
	if(REGTYPE == type && (st->st_size != t->size || (check_mtime && t->mtime != st->st_mtime)))
		return 3;
	return 0;
}

int
enforce_owner(const char *path, const tar_entry *t, struct stat *st)
{
	struct stat st2;
	if(NULL == st) {
		if(lstat(path, &st2) != 0)
			return -1;
		st = &st2;
	}
	if((t->uid != -1 && st->st_uid != t->uid) || (t->gid != -1 && st->st_gid != t->gid)) {
		// lchown time, baby.
		if(lchown(path, t->uid, t->gid) != 0) {
			// shite.
			return -1;
		}
	}
	return 0;
}
	

int
copy_whole_file(cfile *tar_cfh, const tar_entry *ttent) 
{
	// convert to cfile when bufferless write is available.
	int fd;
	off_u32 count;
	off_u32 len;
	cfile_window *cfw;
	struct utimbuf tv;

	v1printf("writing %s\n", ttent->fullname);
	fd = open(ttent->fullname, O_CREAT|O_TRUNC|O_WRONLY, ttent->mode);
	if(fd < 0) {
		v0printf("failed writing %s\n", ttent->fullname);
		return -1;
	} else if(cseek(tar_cfh, ttent->file_loc, CSEEK_FSTART) != ttent->file_loc) {
		v0printf("failed seeking in tarball!\n");
		return -1;
	}
	cfw = expose_page(tar_cfh);
	if(cfw == NULL) {
		v0printf("failed getting next page for %s\n", ttent->fullname);
		return -1;
	}
	len = ttent->size;
	while(len) {
		count = MIN(cfw->end - cfw->pos, len);
		if(count != write(fd, cfw->buff + cfw->pos, count)) {
			v0printf("failed writing to %s\n", ttent->fullname);
			return -1;
		}
		len -= count;
		if(len) {
			cfw = next_page(tar_cfh);
			if(NULL == cfw) {
				v0printf("failed exposing next page for %s\n", ttent->fullname);
				return -1;
			}
		}
	}
	close(fd);
	if(lchown(ttent->fullname, ttent->uid, ttent->gid) != 0) {
		v0printf("failed chown'ing %s\n", ttent->fullname);
		return -1;
	} else if(chmod(ttent->fullname, ttent->mode & 07777) != 0) {
		v0printf("failed chmod'ing %s\n", ttent->fullname);
		return -1;
	}
	tv.actime = start_time;
	tv.modtime = ttent->mtime;
	if(utime(ttent->fullname, &tv) != 0) {
		v0printf("failed setting mtime on %s\n", ttent->fullname);
		return -1;
	}
	return 0;
}

