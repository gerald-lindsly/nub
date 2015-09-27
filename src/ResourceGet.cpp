/*  ResourceGet.cpp -- Codes used only for reading from resource files
    Copyright (c) 2005-2009 by Gerald Lindsly

    See <nub/Platform.h> for additional copyright information
*/

#include "_ResourceFile.h"
#include <nub/imemstream>


void*
ResourceFile::get(const datFilePosType& offset, uint32& size)
{
	UsedHeader head;
	read(&head, sizeof(head), offset);
	byte* buf = new byte[head.uncomp_size];
	if (!buf) throw bad_alloc();
	if (head.comp_size) {
		byte* cbuf = new byte[head.comp_size];
		if (!cbuf) {
			delete buf;
			throw bad_alloc();
		}
        try {
            read(cbuf, head.comp_size);
        }
        catch (...) {
			delete cbuf;
			delete buf;
			throw;
		}
		lzo_uint tsize = head.uncomp_size;
		int r = lzo1x_decompress(cbuf, head.comp_size, buf, &tsize, 0);
		delete cbuf;
		if (r != LZO_E_OK || tsize != head.uncomp_size) {
			delete buf;
            char message[4096];
            sprintf(message, "LZO decompression error on resource file data: %s.1", filename);
			throw io_error(message);
		}
    } else {
        try {
            read(buf, head.uncomp_size);
        }
        catch (...) {
    		delete buf;
		    throw;
        }
	}
	size = head.uncomp_size;
	return buf;
}


bool
ResourceFile::get(const tChar* name, uint32& size, void*& data)
{
  if (!ndx.find(name)) return false;
  data = get(ndx.offset(), size);
  return true;
}


class iresstream : public imemstream
{
public:
    iresstream(void* data, size_t size) :
        imemstream((char*) data, size),
        _data(data)
    {}

    virtual ~iresstream()
    {
        delete _data;
    }

    void* _data;
};


std::istream*
ResourceFile::getStream(const tChar* name)
{
    uint32 size;
    void*  data;
    if (!get(name, size, data)) return 0;
    return new iresstream(data, size);
}


bool
ResourceFile::getSize(const tChar* name, uint32& size, uint32& compressedSize)
{
    if (!ndx.find(name)) return false;
    getSize(ndx.offset(), size, compressedSize);
    return true;
}


void
ResourceFile::getSize(const datFilePosType& offset, uint32& size, uint32& compressedSize)
{
    UsedHeader head;
	read(&head, sizeof(head), offset);
	size = head.uncomp_size;
	compressedSize = head.comp_size;
}

