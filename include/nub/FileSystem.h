/*  <nub/FileSystem.h> -- Header file for basic file manipulation
    Copyright (c) 2015 by Gerald Lindsly

    See <nub/Platform.h> for additional copyright information
*/

#ifndef __NUB_FILESYSTEM_H__
#define __NUB_FILESYSTEM_H__

#define _CRT_SECURE_NO_WARNINGS

#include "Base.h"

namespace nub {

class FileSystem
{
private:
	struct FileInfo;

public:
	typedef FileInfo* FileHandle;

	static FileHandle create(const char* name) // throw (...)
	{
		FILE* f = fopen(name, "w+b");
		return f ? makeFileInfo(f, name) : 0;
	}

	static FileHandle open(const char* name) // throw (...)
	{
		FILE* f = fopen(name, "r+b");
		return f ? makeFileInfo(f, name) : 0;
	}

	static void close(FileHandle fh) {
		fclose(fh->f);
		free(fh->name);
		delete fh;
	}

	static const char* getName(FileHandle fh) {
		return fh->name;
	}

	static void seek(FileHandle fh, int64 pos) // throw (...)
	{
#if NUB_PLATFORM == NUB_PLATFORM_LINUX
		if (fseeko(fh->f, pos, SEEK_SET) == -1)
#else
		fpos_t ofs(pos);
		if (fsetpos(fh->f, &ofs))
#endif
			Throw(fh, "Seek");
	}
	
	static void read(FileHandle fh, void* buffer, int size) // throw(...)
	{
		if (fread(buffer, 1, size, fh->f) != size)
			Throw(fh, "Read");
	}

	static void write(FileHandle fh, void* buffer, int size) // throw(...) 
	{
		if (fwrite(buffer, 1, size, fh->f) != size)
			Throw(fh, "Write");
	}

private:
	struct FileInfo {
		FILE* f;
		char* name;
	};

	static FileHandle makeFileInfo(FILE* f, const char* name) // throw (...)
	{
		FileHandle fh = new FileInfo;
		fh->f = f;
		int len = strlen(name) + 1;
		if (!(fh->name = (char*) malloc(len))) {
			fclose(f);
			delete fh;
			throw bad_alloc();
		}
		memcpy(fh->name, name, len);
		return fh;
	}

	static void Throw(FileHandle fh, const char* reason) // throw (...)
	{
		char msg[1024];
		sprintf(msg, "%s failure on file %s", reason, fh->name);
		fclose(fh->f);
		free(fh->name);
		delete fh;
		throw io_error(msg);
	}
};

} // namespace nub

#endif //  __NUB_FILESYSTEM_H__
