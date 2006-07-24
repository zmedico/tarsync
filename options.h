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
#ifndef _HEADER_OPTIONS
#define _HEADER_OPTIONS 1


#include <getopt.h>

//move this. but to where?
#define EXIT_USAGE -2


struct usage_options {
    char	short_arg;
    char	*long_arg;
    char	*description;
};

#define USAGE_FLUFF(fluff)	\
{0, 0, fluff}

#define OVERSION	'V'
#define OVERBOSE	'v'
#define OUSAGE		'u'
#define OHELP		'h'
#define OSEED		'b'
#define OSAMPLE		's'
#define OHASH		'a'
#define OSTDOUT		'c'
#define OBZIP2		'j'
#define OGZIP		'z'

#define STD_SHORT_OPTIONS					\
"Vvuh"

#define STD_LONG_OPTIONS					\
{"version",		0, 0, OVERSION},			\
{"verbose",		0, 0, OVERBOSE},			\
{"usage",		0, 0, OUSAGE},				\
{"help",		0, 0, OHELP}

#define STD_HELP_OPTIONS					\
{OVERSION, "version", 	"print version"},			\
{OVERBOSE, "verbose", 	"increase verbosity"},			\
{OUSAGE, "usage", 	"give this help"},			\
{OHELP, "help",		"give this help"}


//note no FORMAT_SHORT_OPTION

#define FORMAT_LONG_OPTION(long, short) 				\
{long,	1, 0, short}

#define FORMAT_HELP_OPTION(long,short,description)			\
{short, long, description}

#define END_HELP_OPTS {0, NULL, NULL}
#define END_LONG_OPTS {0,0,0,0}


char *get_next_arg(int argc, char **argv);
void print_version(const char *prog);
void print_usage(const char *prog, const char *usage_portion, struct usage_options *textq, int exit_code);

#endif

