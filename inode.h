/*
 * Copyright (C) 2011 Timo Weingärtner <timo@tiwe.de>
 *
 * This file is part of hadori.
 *
 * hadori is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Foobar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with hadori.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string>
#include <ostream>
#include <fstream>

#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>

#include <zlib.h>

class inode {
public:
	std::string const filename;
	struct stat const stat;
protected:	
	uLong mutable adler;

public:
	inode (std::string const &, struct stat const);
		
	uLong get_adler () const;
	
	friend bool compare (inode const &, inode const &);
	friend std::ostream& operator<< (std::ostream&, inode const &);
};

inline inode::inode (std::string const & __filename, struct stat const __stat) : filename(__filename), stat(__stat), adler(-1) {
}

inline uLong inode::get_adler () const {
	if (adler == uLong(-1)) {
		char buffer[1 << 14];
		std::ifstream f(filename.c_str());
		
		adler = adler32(0L, Z_NULL, 0);
		while (not f.eof()) {
			f.read(buffer, sizeof(buffer));
			adler = adler32(adler, (Bytef *) buffer, f.gcount());
		}
	}
	return adler;
}

inline bool compare (inode const & l, inode const & r) {
	char lbuffer[1 << 14];
	char rbuffer[1 << 14];
	std::ifstream lf(l.filename.c_str());
	std::ifstream rf(r.filename.c_str());
	
	while (not lf.eof()) {
		lf.read(lbuffer, sizeof(lbuffer));
		rf.read(rbuffer, sizeof(rbuffer));
		if (lf.gcount() != rf.gcount())
			return false;
		if (memcmp(lbuffer, rbuffer, lf.gcount()))
			return false;
	}
	return true;
}

inline std::ostream& operator<< (std::ostream& os, inode const & i) {
	os << "Inode " << i.stat.st_ino << ", represented by " << i.filename;
	return os;
}
