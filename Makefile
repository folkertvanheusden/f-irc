# GPLv2 applies
# SVN revision: $Revision: 886 $
# (C) 2006-2014 by folkert@vanheusden.com
VERSION=2.1

DESTDIR=/usr/local
SYSCONFDIR=/etc

DEBUG=-O3 -g -fno-inline -D_DEBUG -D_FORTIFY_SOURCE=2
LDFLAGS+=-lpanelw -lncursesw -pthread -lm -rdynamic
CFLAGS+=-DVERSION=\"$(VERSION)\" -DSYSCONFDIR=\"$(DESTDIR)$(SYSCONFDIR)\" $(DEBUG) -Wall -pedantic

FILES=*.c *.h firc.* f-irc.1 Makefile readme.txt license.txt faq.txt changelog.gz
DOCS=firc.conf changelog.gz readme.txt faq.txt
PACKAGE=fi-$(VERSION).tgz

OBJS=string_array.o main.o theme.o buffer.o channels.o error.o utils.o loop.o term.o tcp.o irc.o user.o names.o config.o dcc.o utf8.o key_value.o wordcloud.o grep_filter.o nickcolor.o chistory.o autocomplete.o checkmail.o servers.o colors.o ansi.o soundex.o ignores.o dictionary.o lf_buffer.o ctcp.o headlines.o help.o scrollback.o script.o xclip.o #mssl.o

all: f-irc

f-irc: $(OBJS)
	$(CC) $(DEBUG) $(OBJS) $(LDFLAGS) -o f-irc

install-docs-base:
	mkdir -p $(DESTDIR)$(PREFIX)/share/doc/f-irc
	cp -a $(DOCS) $(DESTDIR)$(PREFIX)/share/doc/f-irc

install-docs-wlicense: install-docs-base
	cp -a license.txt $(DESTDIR)$(PREFIX)/share/doc/f-irc

install-nodocs: f-irc install-gen

install-debian: f-irc install-gen install-docs-base

install: f-irc install-gen install-docs-wlicense

install-gen:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	rm -f $(DESTDIR)$(PREFIX)/bin/fi
	cp -a f-irc $(DESTDIR)$(PREFIX)/bin/f-irc
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	cp f-irc.1 $(DESTDIR)$(PREFIX)/share/man/man1

deb: package
	rm -rf d
	mkdir -p d/tmp
	cp -a $(FILES) debian/ d/tmp
	cp license.txt d/tmp/debian/copyright
	sed -i 's/^DEBUG=.*/DEBUG=-O3 -g/g' d/tmp/Makefile
	make package
	mv $(PACKAGE) d/f-irc_$(VERSION).orig.tar.gz
	cd d/tmp ; debuild -k'Folkert van Heusden <mail@vanheusden.com>'

clean:
	rm -f $(OBJS) f-irc core log.log gmon.out *.da

package:
	# source package
	rm -rf fi-$(VERSION)*
	mkdir fi-$(VERSION)
	cp $(FILES) fi-$(VERSION)
	sed -i 's/^DEBUG=.*/DEBUG=-O3 -g/g' fi-$(VERSION)/Makefile
	tar czf $(PACKAGE) fi-$(VERSION)
	rm -rf fi-$(VERSION)

daily: package
	scp $(PACKAGE) folkert@vps001.vanheusden.com:www/daily_f-irc/`date +%Y_%m_%d`.tgz

cppcheck:
	cppcheck -v --enable=all --std=c++11 --inconclusive -I. . 2> err.txt
	make clean
	scan-build make

coverity: clean
	rm -rf cov-int
	cov-build --dir cov-int make clean all
	tar vczf ~/site/coverity/f-irc.tgz README cov-int/
	putsite -q
	/home/folkert/.coverity-fi.sh
