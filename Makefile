# Copyright (C) 2011 Timo Weingärtner <timo@tiwe.de>
#
# This file is part of hadori.
#
# hadori is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# hadori is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with hadori.  If not, see <http://www.gnu.org/licenses/>.

LDLIBS=-lboost_program_options -lstdc++
CXXFLAGS?=-O2 -Wall
CXXFLAGS+=-std=c++11
CPPFLAGS+=-D_FILE_OFFSET_BITS=64

all: hadori.1

hadori.1: hadori
	help2man -n $< -o $@ -N ./$<

hadori: hadori.o
hadori.o: hadori.C version.h

version.h:
	test ! -d .git || git describe | sed 's/^\(.*\)$$/#define HADORI_VERSION "hadori \1"/' > $@

clean:
	rm -f hadori hadori.o hadori.1
	test ! -d .git || git checkout -f -- version.h

.PHONY: version.h clean
