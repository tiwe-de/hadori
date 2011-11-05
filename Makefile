LDFLAGS+=-lz -lboost_program_options
CXXFLAGS?=-O2 -Wall
CXXFLAGS+=-std=c++0x
CPPFLAGS+=-D_FILE_OFFSET_BITS=64

all: hadori

hadori.1: hadori
	help2man -n $< -o $@ -N --no-discard-stderr --version-string 0.1 ./$<

hadori: hadori.o
hadori.o: hadori.C inode.h

clean:
	rm -f hadori hadori.o
