tarsync
-------

Tarsync - delta compression suite for using/generating binary patches.

Usage
=====

```shell
tarsync [flags] [options] tarball directory target

  -V --version         print version
  -v --verbose         increase verbosity
  -u --usage           give this help
  -h --help            give this help
  -s --strip-dirs      specify the number of directories to strip from the tarball archive during recreation
  -o --overide-owner   Ensure files has this owner, instead of what the tarball states
  -g --overide-group   Ensure files have this username, instead of what the tarball states
  -e --exclude-fnmatch Specify a glob pattern for excluding files.
  -p --preserve        Enforce tarball permissions, rather then users (modified by -o and -g).
```

`tarsync` expects two args, a tarball (can be compressed), and
a name for the directory to 'sync' up to the tarballs contents.

Example usage:

```shell
tarsync --strip-dir 1 portage-20050511.tar.bz2 /usr/portage
```

Compilation
===========

tarsync requires libcfile library from [diffball](https://github.com/zmedico/diffball) utility.

Just `make` from project's source dir.

License
=======

tarsync licensed under GPL-2+ license. See `LICENSE.txt` for full license text.

```
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

```
