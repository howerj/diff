CFLAGS=-O2 -std=c99 -pedantic -Wall -Wextra -DDIFF_MAIN -g
.PHONY: all default clean

all default: diff sim

clean:
	git clean -dffx
