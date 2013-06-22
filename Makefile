ifndef CC
	CC=gcc
endif
tarsync:	main.o names.o tar.o string-misc.o fs.o options.o excludes.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o tarsync -lcfile
all:		tarsync

clean:	
	-rm -f *.o tarsync

%.o:%.c
	$(CC) -Wall -D_GNU_SOURCE $(CFLAGS) -c $< -o $@

dist:
	@rm -rf .build
	mkdir -p .build/tarsync
	bzr log --verbose > .build/tarsync/ChangeLog
	cp main.c names.c names.h tar.c tar.h string-misc.c string-misc.h fs.c fs.h options.c options.h \
		excludes.c excludes.h Makefile defs.h .build/tarsync/
	cd .build && tar -vjcf ../tarsync.tar.bz2 tarsync
	rm -rf .build

install: tarsync
	mkdir -p ${D}/usr/bin
	install -g 0 -o 0 -m 755 tarsync ${D}/usr/bin
