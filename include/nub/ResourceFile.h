/*  <nub/ResourceFile.h> -- Header file for resource file manipulation
    Copyright (c) 2005-2009 by Gerald Lindsly

    See <nub/Platform.h> for additional copyright information
*/

#ifndef __NUB_RESOURCEFILE_H__
#define __NUB_RESOURCEFILE_H__

#include <nub/Index.h>
#include <istream>

namespace nub {

class _NubExport ResourceFile
{
public:
	typedef int64  datFilePosType;
	typedef uint32 ndxFilePosType;
	typedef IndexT<IKeyASCIIZ, FileSystem, 1024, ndxFilePosType, datFilePosType> ndxFileType;

    ResourceFile() : dat(0), wrkmem(0) {}
	_NubExport ~ResourceFile();

	/// open/create the resource file
	bool open(const char* filename, bool create=false);

	/// close so it can be opened on another file
	void close();

	bool isOpen() { return dat != 0; }

	/// get named/typed data from the archive
    //     you should delete the data when finished with it
	bool get(const char* name, uint32& size, void*& data);

    /// get an istream from the archive
    std::istream* getStream(const char* name);

	/// get size info from the archive without getting the data
	bool getSize(const char* name, uint32& size, uint32& compressedSize);

	/// put named/typed data to the index/data pair
	void put(const char* name, void* data, uint32 size);

	/// copy a file into the archive.  returns false if file not found
	bool putFile(const char* path);

	/// remove named data from index/dat pair
	bool remove(const char* name);

    /// pre-allocate memory for lzo compress
    //    if this is not called, put will allocate the wrkmem every time it is called
	void preCompress();
    /// unallocate lzo compress wrkmem
	void postCompress();

    /// utility to return the index file so you can scan for all entries
    ndxFileType& getIndex() { return ndx; }

// "protected"
	// These two functions bypass the index file.  The data will not
	// be retrievable unless you have an alternate method for remembering
	// the offset returned by put().  get() comes in handy if you index
	// the file by an additional index.

	/// get unnamed data from dat file by offset
	void* get(const datFilePosType& offset, uint32& size);

	/// put unnamed data to dat file. returns a offset for get
	datFilePosType put(void* data, uint32 size);

protected:
	/// get size info without fetching the data
	void getSize(const datFilePosType& offset, uint32& size, uint32& compressedSize);
	/// remove unnamed data from the dat file
	void  remove(const datFilePosType& offset);

    /// internal read - no seek if offset = -1
    void read(void* data, uint32 size, const datFilePosType& offset = -1); // throw(...);
    /// internal write - no seek if offset = -1
    void write(void* data, uint32 size, const datFilePosType& offset = -1); // throw(...);

	FileSystem::FileHandle dat;
	datFilePosType filesize;
	datFilePosType freelist;
	ndxFileType ndx;

	byte* wrkmem;  // workspace for lzo (put, putFile only)

	struct FreeHeader
	{
		uint32 size;         // size of the block (maps to UsedHeader.size)
							 //   includes FreeHeader
		datFilePosType next; // offset of next free block in list
	};

	struct UsedHeader  
	{
		uint32 size;         // bytes reserved in file for block including header
		uint32 comp_size;    // compressed size of data (0 if uncompressed)
		uint32 uncomp_size;  // uncompressed size
	};

};


} // namespace nub

#endif // __NUB_RESOURCEFILE_H__

