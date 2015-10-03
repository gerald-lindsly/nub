/*  ResourceFile.cpp -- Codes common to both reading and writing resource files
    Copyright (c) 2005-2009 by Gerald Lindsly

    See <nub/Platform.h> for additional copyright information
*/

#include "_ResourceFile.h"

//template IndexT<IKeyASCIIZ, FileSystem, 1024, ResourceFile::ndxFilePosType, ResourceFile::datFilePosType>;

struct Init {
	Init() { lzo_init(); }
} _init;


void
ResourceFile::read(void* data, uint32 size, const datFilePosType& offset) // throw(...)
{
    const char* op = 0;
	try {
		if (offset != -1) {
			op = "Seek";
			FileSystem::seek(dat, offset);
		}
		op = "Read";
		FileSystem::read(dat, data, size);
	} catch (...) {
        char message[1024];
        sprintf(message, "%s failure on resource file data: %s", op, FileSystem::getName(dat));
        FileSystem::close(dat);
        dat = 0;
        ndx.close();
        throw io_error(message);
	}
}


void
ResourceFile::write(void* data, uint32 size, const datFilePosType& offset) // throw(...)
{
    const char* op = 0;
	try {
		if (offset != -1) {
			op = "Seek";
			FileSystem::seek(dat, offset);
		}
		op = "Write";
		FileSystem::write(dat, data, size);
	} catch (...) {
        char message[1024];
        sprintf(message, "%s failure on resource file data: %s", op, FileSystem::getName(dat));
        FileSystem::close(dat);
        dat = 0;
        ndx.close();
        throw io_error(message);
    }
}


bool
ResourceFile::open(const char* filename, bool create) // throw(...)
{
    char message[1024];
    bool err = false;
    close();
	size_t len = strlen(filename);
	char* tname = new char[len+3];
    strcpy(tname, filename);
	strcpy(tname + len, ".1");
	dat = create ? FileSystem::create(tname) : FileSystem::open(tname);
	if (!dat) {
        delete tname;
        return false;
    }
	tname[len+1] = '0';
	if (create) {
        try {
            ndx.create(tname, false);
        }
        catch (...) {
			FileSystem::close(dat);
			dat = 0;
            delete tname;
            throw;
        }
    	freelist = 0;
		filesize = sizeof(filesize) + sizeof(freelist);
        write(&filesize, sizeof(datFilePosType));
        write(&freelist, sizeof(datFilePosType));
	} else if (!ndx.open(tname)) {
        sprintf(message, "Index file non-existent for resource file: %s", filename);
        FileSystem::close(dat);
        dat = 0;
        delete tname;
        throw io_error(message);
	} else {
        read(&filesize, sizeof(datFilePosType));
        read(&freelist, sizeof(datFilePosType));
	}
    delete tname;
	return true;
}


void
ResourceFile::close()
{
	if (dat) {
		write(&filesize, sizeof(filesize), 0);
		write(&freelist, sizeof(freelist));
		FileSystem::close(dat);
		dat = 0;
		ndx.close();
	}
}

ResourceFile::~ResourceFile()
{
	close();
}


