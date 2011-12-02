#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <iostream>
#include <sstream>

#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sysexits.h>

#include "inode.h"

// needed for equal_range and range-for
namespace std {
template<typename T> T& begin(pair<T,T> & ip) {
	return ip.first;
}
template<typename T> T& end(pair<T,T> & ip) {
	return ip.second;
}
}

po::variables_map config;
std::ostream debug(std::clog.rdbuf()), verbose(std::clog.rdbuf()), error(std::clog.rdbuf());

void do_link (inode const & i, std::string const & other) {
	if (!link(i.filename.c_str(), other.c_str())) {
		error << "linking " << i << " to " << other << " succeeded before unlinking (race condition)" << std::endl;
		exit(EX_UNAVAILABLE);
	}
	if (errno != EEXIST) {
		char * errstring = strerror(errno);
		error << "error linking " << i << " to " << other << ": " << errstring << ", nothing bad happened." << std::endl;
		exit(EX_UNAVAILABLE);
	}
	if (unlink(other.c_str())) {
		char * errstring = strerror(errno);
		error << "error unlinking " << other << " before linking " << i << " to it: " << errstring << std::endl;
		exit(EX_UNAVAILABLE);
	}
	if (link(i.filename.c_str(), other.c_str())) {
		char * errstring = strerror(errno);
		error << "error linking " << i << " to " << other << ": " << errstring << ", destination filename was already unlinked." << std::endl;
		exit(EX_UNAVAILABLE);
	}
}

void handle_file(std::string const & path, struct stat const & s) {
	static std::unordered_map<ino_t, inode const> kept;
	static std::unordered_map<ino_t, ino_t> to_link;
	static std::unordered_multimap<off_t, ino_t> sizes;
	
	debug << "examining " << path << std::endl;
	if (kept.count(s.st_ino)) {
		debug << "another link to inode " << s.st_ino << " that we keep" << std::endl;
		return;
	}
	if (to_link.count(s.st_ino)) {
		inode const & target = kept.find(to_link[s.st_ino])->second;
		debug << "another link to inode " << s.st_ino << " that we merge with " << target << std::endl;
		do_link(target, path);
		if (s.st_nlink == 1)
			to_link.erase(s.st_ino);
		return;
	}
	inode f(path, s);
	debug << f << " is new to us" << std::endl;
	for (auto const & it : sizes.equal_range(s.st_size)) {
		inode const & candidate = kept.find(it.second)->second;
		debug << "looking if it matches " << candidate << std::endl;
		if (candidate.stat.st_mode != s.st_mode)
			continue;
		if (candidate.stat.st_uid != s.st_uid)
			continue;
		if (candidate.stat.st_gid != s.st_gid)
			continue;
		if (not config.count("no-time"))
			if (candidate.stat.st_mtime != s.st_mtime)
				continue;
		if (config.count("hash"))
			if (candidate.get_adler() != f.get_adler())
				continue;
		if (!compare(candidate, f))
			continue;
		verbose << "linking " << candidate << " to " << path << std::endl;
		if (s.st_nlink > 1)
			to_link.insert(std::make_pair(s.st_ino, it.second));
		if (not config.count("dry-run"))
			do_link(candidate, path);
		return;
	}
	debug << "we keep " << f << std::endl;
	kept.insert(std::make_pair(s.st_ino, f));
	sizes.insert(std::make_pair(s.st_size, s.st_ino));
}

void recurse (std::string const & dir, dev_t const dev) {
	DIR* D;
	struct dirent *d;
	struct stat s;
	std::queue<std::string> subdirs;
	
	if (!(D = opendir(dir.c_str()))) {
		char * errstring = strerror(errno);
		error << "opendir(\"" << dir << "\"): " << errstring << std::endl;
		return;
	}
	while ((d = readdir(D))) {
		std::string path(dir);
		path += '/';
		path += d->d_name;
		if (lstat(path.c_str(), &s)) {
			char * errstring = strerror(errno);
			error << "lstat(\"" << path << "\"): " << errstring << std::endl;
			continue;
		}
		if (s.st_dev != dev) {
			error << path << " resides on another file system, ignoring." << std::endl;
			continue;
		}
		if (S_ISDIR(s.st_mode))
			subdirs.push(d->d_name);
		if (S_ISREG(s.st_mode))
			handle_file(path, s);
	}
	closedir(D);
	// directories get handled after the parent dir is closed to prevent exhausting fds
	for (; !subdirs.empty(); subdirs.pop()) {
		if (subdirs.front() == "." || subdirs.front() == "..")
			continue;
		std::string subdir(dir);
		subdir += '/';
		subdir += subdirs.front();
		recurse(subdir, dev);
	}
}

void recurse_start (std::string const & dir) {
	struct stat s;

	if (lstat(dir.c_str(), &s)) {
		char * errstring = strerror(errno);
		error << "lstat(\"" << dir << "\"): " << errstring << std::endl;
		exit(EX_NOINPUT);
	}
	
	static dev_t const dev = s.st_dev;
	if (dev != s.st_dev) {
		error << dir << " resides on another file system, ignoring." << std::endl;
		return;
	}
	
	if (S_ISDIR(s.st_mode))
		recurse(dir, dev);
	if (S_ISREG(s.st_mode))
		handle_file(dir, s);
}

int main (int const argc, char ** argv) {
	po::options_description opts("OPTIONS");
	opts.add_options()
		("help,h",	"print this help message")
		("no-time,t",	"ignore mtime")
		("hash",	"use adler32 hash to speed up comparing many files with same size and mostly identical content")
		("dry-run,n",	"don't change anything, implies --verbose")
		("verbose,v",	"show which files get linked")
		("debug,d",	"show files being examined")
		("stdin,s",	"read arguments from stdin, one per line")
		("null,0",	"implies --stdin, but use null bytes as delimiter")
		;
	po::options_description all_opts;
	all_opts.add(opts);
	all_opts.add_options()
		("args",	po::value<std::vector<std::string>>(), "files and directories to work on")
		;
	po::positional_options_description pos_opts;
	pos_opts.add("args", -1);
	po::store(po::command_line_parser(argc, argv).options(all_opts).positional(pos_opts).run(), config);
	po::notify(config);
	
	if (config.count("help")) {
		error << "Invocation: hadori [ OPTIONS ] [ ARGUMENTS ]" << std::endl;
		error << opts << std::endl;
		return EX_USAGE;
	}
	
	if (not config.count("debug"))
		debug.rdbuf(nullptr);
	if (not config.count("debug") and not config.count("verbose") and not config.count("dry-run"))
		verbose.rdbuf(nullptr);
	
	if (config.count("args")) {
		if (config.count("stdin") or config.count("null")) {
			// not supported because we don't know which arguments to scan first
			error << "--stdin combined with commandline arguments, this is not supported." << std::endl;
			return EX_USAGE;
		}
		for(auto const & dir : config["args"].as<std::vector<std::string>>())
			recurse_start(dir);
	} else {
		if (not config.count("stdin") and not config.count("null"))
			error << "no arguments supplied, assuming --stdin." << std::endl;
		char delim = '\n';
		if (config.count("null"))
			delim = '\0';
		for (std::string dir; getline(std::cin, dir, delim);)
			recurse_start(dir);
	}

	return EX_OK;
}
