CC = gcc
#CC = clang
#CC = tcc

CFLAGS = -Wall -Wextra -O3 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE
LDFLAGS = -lm -pie

SRCS = txtfmt.c help.c tag_handler.c tags.c tags_lib.c tinyexpr.c
OBJS = $(SRCS:.c=.o)

all: txtfmt

txtfmt: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o txtfmt

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f txtfmt $(OBJS)

PREFIX ?= /usr/local

install: txtfmt
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755 txtfmt $(DESTDIR)$(PREFIX)/bin/txtfmt

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/txtfmt

.PHONY: all clean install uninstall 
