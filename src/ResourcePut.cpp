/*  ResourcePut.cpp -- Codes used only for writing to resource files
    Copyright (c) 2005-2009 by Gerald Lindsly

    See <nub/Platform.h> for additional copyright information
*/

#include "_ResourceFile.h"


#define X999 // configuation flag to use expensive best compression
             //   comment out to use faster and cheaper (memory-wise) compression

#ifdef X999
#   define MEM_COMPRESS LZO1X_999_MEM_COMPRESS
#   define COMPRESS lzo1x_999_compress
#else
#   define MEM_COMPRESS LZO1X_1_MEM_COMPRESS
#   define COMPRESS lzo1x_1_compress
#endif

typedef ResourceFile::datFilePosType datFilePosType;

datFilePosType
ResourceFile::put(void* data, uint32 size)
{
	byte* comp = new byte[size + size / 16 + 64 + 3];	// allocate compressed data buffer

// get wrkmem for lzo
	bool preallocated = wrkmem != 0;
	if (!preallocated) preCompress();

	UsedHeader usedHead;
	usedHead.uncomp_size = size;

// compress
	lzo_uint comp_size;
	COMPRESS((const lzo_byte*)data, size, comp, &comp_size, wrkmem);

#ifdef X999
	lzo_uint comp2_size = size;
	lzo1x_optimize(comp, comp_size, (lzo_byte*)data, &comp2_size, wrkmem);
#endif

	if (comp_size < size) {
		usedHead.comp_size = comp_size;
		size = comp_size; // set size of block to search for
		data = comp;  // use compressed data
	} else // unable to compress
		usedHead.comp_size = 0;
	FreeHeader freeHead; 	// block header in file
	// search the free list for block big enough for size
	datFilePosType offset = freelist;
	datFilePosType prevOfs = 0;
	FreeHeader prevHead;
	while (offset) {
        read(&freeHead, sizeof(FreeHeader), offset);
		if (freeHead.size >= size + sizeof(UsedHeader)) {
			// found a block big enough
			if (freeHead.size <= size + sizeof(UsedHeader) + sizeof(FreeHeader)) {
				// not big enough to split
				usedHead.size = freeHead.size;
				if (prevOfs) {
					prevHead.next = freeHead.next;
					write(&prevHead, sizeof(FreeHeader), prevOfs);
				} else
					freelist = freeHead.next;
			} else {
				// split the free block
				freeHead.size -= usedHead.size = sizeof(UsedHeader) + size;
                write(&freeHead.size, sizeof(datFilePosType), offset);
				offset += freeHead.size;
			}
			break;
		}
		prevOfs = offset;
		prevHead.size = freeHead.size;
		offset = freeHead.next;
	}

	if (!offset) {
		// not found, append to end of file
		try {
			FileSystem::seek(dat, filesize);
		} catch (...) {
            char message[1024];
            sprintf(message, "Seek error in resource file data: %s", FileSystem::getName(dat));
            FileSystem::close(dat);
            dat = 0;
            ndx.close();
            throw io_error(message);
        }
		usedHead.size = size + sizeof(UsedHeader);
		offset = filesize;
	}
	write(&usedHead, sizeof(UsedHeader), offset);
	write(data, size);
	delete comp;
	if (!preallocated) postCompress();
	return offset;
}


void
ResourceFile::put(const char* name, void* data, uint32 size)
{
	datFilePosType offset;
	if (ndx.find(name)) {
		remove(ndx.offset());
		offset = put(data, size);
		ndx.change(offset);
	} else {
		offset = put(data, size);
		ndx.insert(name, offset);
	}
}


bool
ResourceFile::putFile(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f) return false;

    static char* seekOp = "Seek";
    byte* data = 0;
    char* failedOp = 0;
	if (fseek(f, 0, SEEK_END) != 0) failedOp = seekOp;
	uint32 len = ftell(f);
	if (fseek(f, 0, SEEK_SET) != 0) failedOp = seekOp;
	
	try {
		data = new byte[len];
	} catch (...) {
        fclose(f);
        throw;
    }

    if (fread(data, 1, len, f) != len) failedOp = "Read";

    if (failedOp) {
        if (data) delete data;
        fclose(f);
        char message[1024];
        sprintf(message, "%s failed on file %s", failedOp, path);
        throw io_error(message);
    }

    put(path, data, len);
	delete data;
	fclose(f);
    return true;
}


void
ResourceFile::remove(const datFilePosType& offset)
{
	FreeHeader thisHead;
	read(&thisHead.size, sizeof(thisHead.size), offset);

	if (freelist) {
	// scan free list to see if we can merge other free areas with this one
		datFilePosType thisOfs = offset;
		datFilePosType freeOfs = freelist;
		FreeHeader     prevHead;
		datFilePosType prevOfs = 0;
		do {
			FreeHeader freeHead;
			read(&freeHead, sizeof(FreeHeader), freeOfs);
			if (freeOfs + freeHead.size == thisOfs) {
				// previous block merges with this
				thisHead.size += freeHead.size;
				thisOfs = freeOfs;
				freeOfs = thisHead.next = freeHead.next;
				// scan the rest of the blocks to see if the adjacent forward
				//  block is free too
				if (freeOfs) {
					datFilePosType prev2Ofs = 0;
					FreeHeader prev2Head;
					do {
						read(&freeHead, sizeof(FreeHeader), freeOfs);
						if (thisOfs + thisHead.size == freeOfs) {
							thisHead.size += freeHead.size;
							if (prev2Ofs) {
								prev2Head.next = freeHead.next;
								write(&prev2Head, sizeof(FreeHeader), prev2Ofs);
							} else
								thisHead.next = freeHead.next;
							break;
						}
						prev2Head.size = freeHead.size;
						prev2Ofs = freeOfs;
						freeOfs = freeHead.next;
					} while (freeOfs);
				}
  				write(&thisHead, sizeof(FreeHeader), thisOfs);
				return;
			} else if (thisOfs + thisHead.size == freeOfs) {
				// next block merges with this
				thisHead.size += freeHead.size;
				freeOfs = thisHead.next = freeHead.next;
				// scan the rest of the blocks to see if the adjacent backward
				//  block is free too
				if (freeOfs) {
					datFilePosType prev2Ofs = 0;
					FreeHeader prev2Head;
					do {
						read(&freeHead, sizeof(FreeHeader), freeOfs);
						if (freeOfs + freeHead.size == thisOfs) {
							thisHead.size += freeHead.size;
							thisOfs = freeOfs;
							if (prev2Ofs) {
								prev2Head.next = freeHead.next;
								write(&prev2Head, sizeof(FreeHeader), prev2Ofs);
							}
							else
								thisHead.next = freeHead.next;
							break;
						}
						prev2Head.size = freeHead.size;
						prev2Ofs = freeOfs;
						freeOfs = freeHead.next;
					} while (freeOfs);
				}
  				write(&thisHead, sizeof(FreeHeader), thisOfs);
				if (prevOfs) {
					prevHead.next = thisOfs;
					write(&prevHead, sizeof(FreeHeader), prevOfs);
				} else
					freelist = thisOfs;
				return;
			}
			prevHead.size = freeHead.size;
			prevOfs = freeOfs;
			freeOfs = freeHead.next;
		} while (freeOfs);
	};
	thisHead.next = freelist;
	freelist = offset;
	write(&thisHead, sizeof(FreeHeader), offset);
}


bool
ResourceFile::remove(const tChar* name)
{
	datFilePosType offset = ndx.find(name);
	if (!offset)
		return false;
	remove(offset);
	return ndx.remove_current();
}


void
ResourceFile::preCompress()
{
    if (!wrkmem) {
        wrkmem = new byte[MEM_COMPRESS];
        if (!wrkmem) throw bad_alloc();
    }
}


void
ResourceFile::postCompress()
{
	NUB_DELETE(wrkmem);
}
