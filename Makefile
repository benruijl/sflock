# sflock - simple feedback screen locker
# © 2010-2011 Ben Ruijl
# Based on sflock
# © 2006-2007 Anselm R. Garbe, Sander van Dijk

include config.mk

SRC = sflock.c
OBJ = ${SRC:.c=.o}

all: options sflock

options:
	@echo sflock build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

sflock: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f sflock ${OBJ} sflock-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p sflock-${VERSION}
	@cp -R LICENSE Makefile README config.mk ${SRC} sflock-${VERSION}
	@tar -cf sflock-${VERSION}.tar sflock-${VERSION}
	@gzip sflock-${VERSION}.tar
	@rm -rf sflock-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f sflock ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/sflock
	@chmod u+s ${DESTDIR}${PREFIX}/bin/sflock

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/sflock

.PHONY: all options clean dist install uninstall
