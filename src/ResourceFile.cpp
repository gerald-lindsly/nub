/*  ResourceFile.cpp -- Codes common to both reading and writing resource files
    Copyright (c) 2005-2009 by Gerald Lindsly

    See <nub/Platform.h> for additional copyright information
*/

#include "_ResourceFile.h"

template IndexT<IKeyASCIIZ, FileSystem, 1024, ResourceFile::ndxFilePosType, ResourceFile::datFilePosType>;

struct Init {
	Init() { lzo_init(); }
} _init;


void
ResourceFile::read(void* data, uint32 size, const datFilePosType& offset) throw(...)
{
    char* failedOp = 0;
	if (offset != -1) {
		const fpos_t ofs(offset);
		if (fsetpos(dat, &ofs)) failedOp = "Seek";
	}
    if (!failedOp && fread(data, 1, size, dat) != size) failedOp = "Read";
    if (failedOp) {
        char message[4096];
        sprintf(message, "%s failure on resource file data: %s.1", failedOp, filename);
        fclose(dat);
        dat = 0;
        delete filename;
        ndx.close();
        throw io_error(message);
    }
}


void
ResourceFile::write(void* data, uint32 size, const datFilePosType& offset) throw(...)
{
    char* failedOp = 0;
	if (offset != -1) {
		const fpos_t ofs(offset);
		if (fsetpos(dat, &ofs)) failedOp = "Seek";
	}
    if (!failedOp && fwrite(data, 1, size, dat) != size)  failedOp = "Write";
    if (failedOp) {
        char message[4096];
        sprintf(message, "%s failure on resource file data: %s.1", failedOp, filename);
        fclose(dat);
        dat = 0;
        delete filename;
        ndx.close();
        throw io_error(message);
    }
}


bool
ResourceFile::open(const char* _filename, bool create) throw(...)
{
    char message[4096];
    bool err = false;
    close();
	size_t len = strlen(_filename);
	filename = new char[len+1];
	strcpy(filename, _filename);
	char* tname;
	try {
		tname = new char[len+3];
	} catch (...) {
        delete filename;
        throw;
    }
    strcpy(tname, filename);
	strcpy(tname + len, ".1");
    if (!(dat = fopen(tname, create ? "w+b" : "r+b"))) {
        delete tname;
        delete filename;
        return false;
    }
	tname[len+1] = '0';
	if (create) {
        try {
            ndx.create(tname, false);
        }
        catch (...) {
			fclose(dat);
			dat = 0;
            delete tname;
            delete filename;
            throw;
        }
    	freelist = 0;

        write(&freelist, sizeof(datFilePosType));
	} else if (!ndx.open(tname)) {
        sprintf(message, "Index file non-existent for resource file: %s", filename);
        fclose(dat);
        dat = 0;
        delete tname;
        delete filename;
        throw io_error(message);
	} else
        read(&freelist, sizeof(datFilePosType));
    delete tname;
	return true;
}


void
ResourceFile::close()
{
	if (dat) {
		write(&freelist, sizeof(freelist), 0);
		fclose(dat);
		dat = 0;
		ndx.close();
	}
}

ResourceFile::~ResourceFile()
{
	close();
}


