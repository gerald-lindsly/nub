/*  <nub/Index.h> -- Header file for manipulating index files
    Copyright (c) 2005-2015 by Gerald Lindsly

    See <nub/Platform.h> for additional copyright information

    The original source for this code was NDX 3.1,
    Copyright (C) 1987-89, 1992-93 by George H. Mealy.

    The code presented here is not written entirely from scratch, but has essentially
    been totally rewritten.  I have made so many improvements, data structure changes
    and bug fixes that this code is generally unrecognizable when compared to the original.

*/
/*	<BD-Calvin> Gero: fixed __int64 in Platform.h, removed all exception specifications, 
		compiled with -fpermissive (const correctness issues), changed an fsetpos to fseek.

	<TinoDidriksen> You don't need COM for C# - you can call native code with P/Invoke.
	
	<BD-Calvin> http://symas.com/mdb/
<BD-Calvin> https://github.com/google/leveldb
<BD-Calvin> bdb, rocksdb, even sqlite
*/

#ifndef __NUB_INDEX_H__
#define __NUB_INDEX_H__

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stddef.h>

#include "Base.h"
#include "FileSystem.h"

namespace nub {

const byte ndxMAJOR = 6;      // Version numbers of the index files
const byte ndxMINOR = 0;
const int  ndxMaxStack = 64;  // Maximum tree height


/* Exception specifictions removed as of 0.3.3 
       throw(...) with no comment did indicates that the function can throw
      io_error      - Usually either disk full or the index file is corrupted.
      runtime_error - Index is too deep.  Probably indicates corrupted index file.
                      See ndxMaxStack.  Default of 64 should be impossible to overflow (M-way branching)
   Otherwise (when a comment is given) the function should throw only those exceptions listed.
*/

#pragma pack(push, 1)

//#define FIELDOFFSET(type, field) ((size_t)&(((type*)0)->field))
#define FIELDOFFSET(type, field) (offsetof(type, field))


class IKeyASCIIZ
// also works to some degree for UTF-8, but the sorting order may be off
{
public:
	static int size(const void* key)
		{ return (int)strlen((char*)key) + 1; }

	static int compare(const void* lhs, const void* rhs)
		{ return strcmp((char*)lhs, (char*)rhs); }

	static void copy(void* target, const void* source)
		{ strcpy((char*)target, (char*)source); }

	static const char* toString(const void *key)   { return (const char*) key; }
	// Used only for error reporting

	static const void* emptyKey()                  { return ""; }
	static int   emptyKeySize()                    { return 1; }
};


class IKeyUTF16 // Unicode keys
{
public:
	static int size(const void* key)
	{
		const wchar_t* eos = (const wchar_t*)key;
        while (*eos++)
			;
        return (int)((byte*)eos - (byte*)key);
	}

	static int compare(const void* lhs, const void* rhs)
		{ return wcscmp((wchar_t*)lhs, (wchar_t*)rhs); }

	static void copy(void* target, const void* source)
		{ wcscpy((wchar_t*)target, (wchar_t*)source); }

	static const char* toString(const void *key)   { return "Unicode"; }
	// Used only for error reporting

	static const void* emptyKey()                  { return L""; }
	static int   emptyKeySize()                    { return sizeof(wchar_t); }
};


template <class    IKey        = IKeyASCIIZ,
          typename FileSystemT = FileSystem,
          int      nNodeSize   = 512,
		  typename ndxFilePosT = uint32,
		  typename datFilePosT = uint32>
class IndexT
{
public:
	typedef IKey        IKeyType;
	typedef FileSystemT FileSystemType;
	typedef ndxFilePosT ndxFilePosType;
	typedef datFilePosT datFilePosType;

protected:
	class KeyEntry
	{
	public:
		ndxFilePosT lson;   // Pointer to left son (seek address in file)
		datFilePosT offset; // Record seek address or number associated with key
		byte        key[1]; // the actual key

		int size() const    // Total size of the KeyEntry
			{ return (int)(FIELDOFFSET(KeyEntry, key) + IKey::size(key)); }

		KeyEntry* copy(KeyEntry* k)
			{
				lson = k->lson;
			    offset = k->offset;
				IKey::copy(key, k->key); 
				return this;
		    }
	};

	class Node
	{
	public:
		int32 count;       // # of keys in node
		union {
			ndxFilePosT lson;  // left son node offset in index file
			KeyEntry    key0;  // 1st key
		};
		byte moreKeys[nNodeSize              // The rest of the KeyEntry structures
					  - sizeof(int32)        // count 
					  - sizeof(KeyEntry)];   // Key0
		// rson immediately follows the packed keys (could be lson of next added key)

		// only the above data is stored on disk

		uint16 keyofs[1];  // Offset of keys from the beginning of the node
						   // note: this array is accessed with negative indices
						   // to access the offsets of keys within the node.
						   // The key data grows upwards while the offsets to the keys grows downwards
						   // [0] is not stored in the node on disk (always FIELDOFFSET(Node,Key0))

		byte     filler[sizeof(ndxFilePosT)-sizeof(uint16)];  // align following offset

		ndxFilePosT offset; // Node offset within the index file
		bool     dirty;     // true if node has been modified and needs to be written to disk

		/// get pointer to Ith key
		KeyEntry* keyI(int i) { return (KeyEntry*)((byte*)this + keyofs[-i]); }

		/// Get pointer to right son
		ndxFilePosT* rson() { return &(keyI(count)->lson); }

		/// Split node in two
		int	split(IndexT* ndx) // throw(...)
		{
			// Find entry to be moved up a level
			uint16 endkeys = keyofs[-count];
			uint16 m = (endkeys - (uint16)FIELDOFFSET(Node, key0)) / 2 + (uint16)FIELDOFFSET(Node, key0); // peek into the middle of the keys
			// (m points somewhere in the middle of the key to use for a pivot)
			// find 1st key past pivot
			int i;
			for (i = 1; i < count; i++)
				if (keyofs[-i] >= m)
					break;
			// i was incremented 1 past pivot key
			uint16 pivoto = keyofs[1-i];
			uint16 moveo  = keyofs[-i]; // moveo is offset of keys to move to new node
			uint16 pivotlen = moveo - pivoto;

			Node*     parent;
			KeyEntry* parentk;
			int       parenti;
			if (ndx->stacktop) {
			// If parent full, split it, then we will be back
				parent = ndx->pop(parentk, parenti);
				if (pivotlen +                       // the pivot to put in parent
					sizeof(uint16) +				 // pivot's keyofs for parent
					parent->keyofs[-parent->count] + // parent's key data
					parent->count * sizeof(uint16) + // & keyofs's
					sizeof(ndxFilePosT) >            // rson
					   nNodeSize)
				{
					parent->split(ndx);
					return -1;
				}
			} else {
			// If it was the root, create a new one
				parent = ndx->newNode();
				parentk = &parent->key0;
				parenti = 0;
				ndx->root = parent->offset;
			}

			// create a new node and move the keys after the pivot to it
			Node* added = ndx->newNode();
			memcpy(&added->key0, (byte*)this + moveo, endkeys - moveo + sizeof(ndxFilePosT));

			// set the key offsets within the added node
			moveo -= (uint16)FIELDOFFSET(Node, key0);
			int j = i + 1;
			uint16* w = added->keyofs - 1;
			uint16* w1 = keyofs - j;
			for (; j <= count; j++)
				*w-- = *w1-- - moveo;
			added->count = count - i;
			count -= added->count + 1;
			dirty = true;

			// make room for the pivot in parent
			memmove((byte*)parentk + pivotlen, parentk, 
					parent->keyofs[-parent->count] - ((byte*)parentk - (byte*)parent) + sizeof(ndxFilePosT));
			j = ++parent->count;
			w = parent->keyofs - j;
			while (j > parenti) {
				*w++ = w[1] + pivotlen;
				j--;
			}
			// put the pivot in the parent
			memcpy(parentk, (byte*)this + pivoto, pivotlen);
			parentk->lson = offset;  // point the pivot's lson to this node
			// point the lson of key after pivot to added node
			((KeyEntry*)((byte*)parentk + pivotlen))->lson = added->offset;
			parent->dirty = true;
			return 0;
		}
	};

	class StackFrame // State stack frame
	{
	public:
		ndxFilePosT offset;    // offset of node in the index file
		int i;            // # of key in the node
	};

public:
    /// Constructor.
	IndexT(int maxCache=10) : // throw(...) :   // can throw bad_alloc
		f(0), cacheUsed(0), stacktop(0), n(0), nMaxCache(maxCache)
	{
		const int cNodeExtra  = sizeof(int32)        // Overhead per node: count &
							  + sizeof(ndxFilePosT); // rson
		const int cMaxKeyData = nNodeSize            // Maximum key data in a node
							  - cNodeExtra; 
		const int cKeyExtra   = sizeof(ndxFilePosT)  // lson
							  + sizeof(datFilePosT)  // offset
							  + sizeof(uint16);      // keyofs
		// need room for at least 3 keys in a node so split() will work
		nMaxKeySize = cMaxKeyData/3 - cKeyExtra;
		curKey = new byte[nMaxKeySize];
		clearCurKey();
		cache = new Node*[maxCache];
		Node** c = cache;
		Node* node;
		for (int i = 0; i < nMaxCache; i++) {
			node = new Node;
			*c++ = node;
			node->keyofs[0] = (uint16)FIELDOFFSET(Node, key0);
		}
	}

    /// Destructor, closes index
	~IndexT() // throw(...)
	{
		close();
		Node** c = cache;
		for (int i = 0; i < nMaxCache; i++)
			delete *c++;
		delete[] cache;
	}

    /// Create new index
	void create(char* name, bool _dups=false) // throw(...)  // can throw bad_alloc or io_error
	{
		if (f) FileSystemT::close(f);
		resetCache();
		f = FileSystemT::create(name);

		stacktop = 0;
		major = ndxMAJOR;
		minor = ndxMINOR;
		hNdxPosSize = sizeof(ndxFilePosT);
		hDatPosSize = sizeof(datFilePosT);
		hMaxKeySize = nMaxKeySize;
		hNodeSize = nNodeSize;
		root = eof = nNodeSize;
		freelist = 0;
		n = 0;
		dups = _dups;
		write(0, &major, FIELDOFFSET(IndexT, stacktop) - FIELDOFFSET(IndexT, major));  // Write virgin file header
		newNode();
	}

    /// Open existing index.  Returns false if file does not exist
	bool open(char* name) // throw(...)  // can throw bad_alloc or io_error
	{
		if (f) FileSystemT::close(f);
		resetCache();
		stacktop = 0;

		f = FileSystemT::open(name);
		if (!f) return 0;
		read(0, &major, FIELDOFFSET(IndexT, stacktop) - FIELDOFFSET(IndexT, major));     // Read the index file header
		char message[1024] = "";
		if (major != ndxMAJOR)
			sprintf(message, "Index file major version number is not the expected %d, but %d. %s", ndxMAJOR, major, name);
		else if (hNodeSize != nNodeSize)
			sprintf(message, "Index file NodeSize (%x) in %s does not match compiled code (%x)", hNodeSize, name, nNodeSize);
		else if (hNdxPosSize != sizeof(ndxFilePosT))
			sprintf(message, "Index file offsets in %s are %d bytes instead of the compiled (%d)", name, hNdxPosSize, sizeof(ndxFilePosT));
		else if (hDatPosSize != sizeof(datFilePosT))
			sprintf(message, "Data file offsets in %s are %d bytes instead of the compiled (%d)", name, hDatPosSize, sizeof(datFilePosT));
		else if (hMaxKeySize != nMaxKeySize)
			sprintf(message, "MaxKeySize (%x) in %s does not match compiled code (%x)", hMaxKeySize, name, nMaxKeySize);
		if (message[0]) {
			FileSystemT::close(f);
			f = 0;
			throw io_error(message);
		}
		return true;
	}

    /// Close the index in order to open another
	void close() // throw(...) // can throw io_error
	{
		if (f) {
			Node** c = cache;
			Node*  node;
			int i;
			for (i = 0; i < cacheUsed; i++) {
				node = *c++;
				if (node->dirty)
					write(node->offset, node, nNodeSize);
			}
			write(0, &major, FIELDOFFSET(IndexT, stacktop) - FIELDOFFSET(IndexT, major));
			FileSystemT::close(f);
			f = 0;
			n = 0;
		}
	}

    /// Return file size (# of keys)
	int count() const // noexcept // throw()
		{ return n; }
	const int maxKeySize() const // noexcept // throw ()
		{ return nMaxKeySize; }


    /// Test index validity (true if valid)
	bool valid() // throw(...)
	{
		if (!f) return false;
		if (!first() && n == 0) return true;
		if (n == 0) return false;
		int i;
		void* k = new byte[nMaxKeySize];
		IKey::copy(k, curKey);
		for (i = 1; i < n && next(); i++) {
			if (IKey::compare(k, curKey) > 0) {
				delete[] k;
				return false;
			}
			IKey::copy(k, curKey);
		}
		delete[] k;
   		return i == n && !next();
	}

    /// Return current key (and data offset)
	const void* key() const // noexcept // throw()
		{ return curKey; }
	datFilePosT offset() const // noexept // throw()
		{ return curOffset; }

    /// Insert a new key (and data offset).  Can return false if key already exists and no duplicates allowed
	bool insert(const void* key, const datFilePosT& offset) // throw(...)  // can throw io_error, runtime_error or ivalid_argument (key too long)
	{
		if (!f) return false;

		uint16 size = IKey::size(key);
		if (size > nMaxKeySize) {
			char message[4096];
			sprintf(message, "Key (%s) too long (must be <= %d bytes)", IKey::toString(key), nMaxKeySize);
			throw invalid_argument(message);
		}
		memcpy(curKey, key, size);
		size += (uint16)(FIELDOFFSET(KeyEntry, key));
		curOffset = offset;

		int result;
		do {
			stacktop = 0;
			result = _insert(size, root);
		} while (result < 0);

		if (result) n++;
		return result != 0;
	}

	/// Find a key.  If duplicates are allowed, finds the first instance of a key (lowest data offset).
    //   Note that duplicates are in sorted order by data offset
	bool find(const void* key) // throw(...)
	{
		if (!f) return false;
		stacktop = 0;
		if (!dups) {
			bool ret = _find(key, root);    // Inner routine
			if (!ret) _findNext();
			return ret;
		} else {
			_find(key, ((datFilePosT)-1 >> 1) + 1, root);  // should be 0x80000000 for int32 (most negative number)
			_findNext();
			return IKey::compare(key, curKey) == 0;
		}
	}

    /// Find key, specific record
	bool find(const void* key, const datFilePosT& offset) // throw(...)
	{
		if (!f) return false;
		stacktop = 0;
		bool ret;
		if (dups)
			ret = _find(key, offset, root);    // Inner routine
		else {
			ret = _find(key, root);
			if (ret && offset != curOffset)
				ret = false; 
		}
		if (!ret) _findNext();
		return ret;
	}

    /// Change the data offset of the current key
	bool change(const datFilePosT& offset) // throw(...) // can throw io_error or logic_error (no current key)
	{
		if (!f) return false;
		if (!stacktop) {
			char message[1024];
			sprintf(message, "Stack underflow: no current key. File: %s", FileSystemT::getName(f));
			throw logic_error(message);
		}
		KeyEntry* k;
		int i;
		Node* node = top(k, i);
		curOffset = k->offset = offset;
		node->dirty = true;
		return true;
	}

    /// Remove a key.  Only first instance of a key is removed when there are duplicates.
    //      current key is set to the key following the removed key
	bool remove(const void* key) // throw(...)
	{
		if (!find(key)) return false;
		return remove_current();
	}

    /// Remove a key with a given offset
    //      current key is set to the key following the removed key
	bool remove(const void* key, const datFilePosT& offset) // throw(...)
	{
		if (!find(key, offset)) return false;
		return remove_current();
	}

    /// Remove the current key
    //      current key is set to the key following the removed key
	bool remove_current() // throw(...)
	{
		if (!f) return false;
		if (!stacktop) return false;

		KeyEntry* k;
		int       ttop = stacktop;
		int       i, j, moveo;

		Node* node = pop(k, i);
		if (i == node->count) return false;
		int klen = (moveo = node->keyofs[-i-1]) - node->keyofs[-i];
		if (!k->lson) {              // Key is simply deleted
			memmove(k, (byte*)k + klen,
					node->keyofs[-node->count] - moveo + sizeof(ndxFilePosT));
			uint16* w = node->keyofs - i - 1;
			for (j = i + 1; j < node->count; j++)
				*w-- = w[-1] - klen;
			node->dirty = true;
			if (!--node->count && stacktop) {
				ndxFilePosT son = k->lson;
				freeNode(node);
				KeyEntry* pk;
				Node* parent = top(pk, j);
				pk->lson = son;
				parent->dirty = true;
				if (son) {
					node = getNode(son);
					k = &node->key0; // i must be 0
				}
			}
			// just deleted a key -- see if we can combine sibling nodes
			if (stacktop) {
				moveo = node->keyofs[-node->count];
				int nodeSize = moveo + // node key data
						       sizeof(ndxFilePosT) +        // rson
				               node->count * sizeof(uint16);// keyofs's
				if (nodeSize <= nNodeSize/2) {
					KeyEntry* pk;
					Node* parent = top(pk, j);
					if (j < parent->count) {
					    int pkSize = parent->keyofs[-j-1] - parent->keyofs[-j];
						KeyEntry* q = (KeyEntry*)((byte*)pk + pkSize);
						if (q->lson) { // if parent key has a right child
							Node* rsib = getNode(q->lson);
							int rsibSize = rsib->keyofs[-rsib->count];
							if (nodeSize + pkSize + sizeof(uint16) +
								rsibSize - FIELDOFFSET(Node, key0) +
								rsib->count * sizeof(uint16)
								<= nNodeSize)
							{	// move parent key to end of this node
								// leave rson of node alone (will be lson of new parent key)
								memcpy((byte*)node + moveo + sizeof(ndxFilePosT),
									   &pk->offset, pkSize - sizeof(ndxFilePosT));
								uint16* w = &node->keyofs[-(node->count+1)];
								uint16  y;
								*w-- = y = moveo + pkSize;

								// put rsib keys after parent key
								memcpy((byte*)node + moveo + pkSize,
									   &rsib->key0,
									   rsibSize - FIELDOFFSET(Node, key0) + sizeof(ndxFilePosT));
								uint16* x = &rsib->keyofs[-1];
								y -= FIELDOFFSET(Node, key0);
								for (int r = 0; r < rsib->count; r++)
									*w-- = *x-- + y;
								node->count += 1 + rsib->count;
								freeNode(rsib);

								// remove parent key from parent
								memmove(&pk->offset, // leave ptr to node
										(byte*)&pk->offset + pkSize,
										parent->keyofs[-parent->count] - parent->keyofs[-(j+1)]);
								int jj = j+1;
								for (w = &parent->keyofs[-jj]; jj < parent->count; jj++, w--)
									*w = w[-1] - pkSize;
								parent->dirty = true;

								if (!--parent->count) {
									freeNode(parent);
									if (--stacktop) {
										// grandparent is now parent
										parent = top(pk, j);
										parent->lson = node->offset;
										parent->dirty = true;
									} else
										root = node->offset;
								}
								// recalculate for possible merge left
								moveo = node->keyofs[-node->count];
								nodeSize = moveo + // node key data
										   sizeof(ndxFilePosT) +        // rson
										   node->count * sizeof(uint16);// keyofs's
							}
						}
					}
					if (stacktop && j > 0) {
						pk = parent->keyI(--j);
						if (pk->lson) {
							int pkSize = parent->keyofs[-j-1] - parent->keyofs[-j];
							Node* lsib = getNode(pk->lson);
							int lsibSize = lsib->keyofs[-lsib->count];
							if (lsibSize + lsib->count * sizeof(uint16) +
								pkSize + sizeof(uint16) +
								nodeSize - FIELDOFFSET(Node, key0) 
								<= nNodeSize)
							{	// move parent key to end of lsib
								// leave rson of lsib alone (will be lson of new parent key)
								memcpy((byte*)lsib + lsibSize + sizeof(ndxFilePosT),
									   &pk->offset, pkSize - sizeof(ndxFilePosT));
								uint16* w = &lsib->keyofs[-(lsib->count+1)];
								uint16  y;
								*w-- = y = lsibSize + pkSize;

								// put rsib keys in lsib after parent key
								memcpy((byte*)lsib + lsibSize + pkSize,
									   &node->key0,
									   moveo - FIELDOFFSET(Node, key0) + sizeof(ndxFilePosT));
								uint16* x = &node->keyofs[-1];
								y -= FIELDOFFSET(Node, key0);
								for (int r = 0; r < node->count; r++)
									*w-- = *x-- + y;

								// remove parent key from parent
								memmove(&pk->offset, // leave ptr to node
										(byte*)&pk->offset + pkSize,
										parent->keyofs[-parent->count] - parent->keyofs[-(j+1)]);
								int jj = j+1;
								for (w = &parent->keyofs[-jj]; jj < parent->count; jj++, w--)
									*w = w[-1] - pkSize;

								i += 1 + lsib->count;
								lsib->count += 1 + node->count;
								lsib->dirty = true;
								freeNode(node);

								stack[stacktop-1].i = j;

								node = lsib;
								k = node->keyI(i);

								parent->dirty = true;
								if (!--parent->count) {
									freeNode(parent);
									if (--stacktop) {
										parent = top(pk, j);
										parent->lson = node->offset;
										parent->dirty = true;
									} else
										root = node->offset;
								}
							}
						}
					}
				}
			}

			// If at end of node, up to next key
			if (i == node->count && !k->lson) {
				do {
					if (!stacktop) {
						n--;
						clearCurKey();
						return true;
					}
					node = pop(k, i);
				} while (i == node->count);
				push(node, i);
			} else {
				push(node, i);
				while (k->lson) {            // Get lowest left son, if any
					node = getNode(k->lson);
					k = &node->key0;
					push(node, 0);
				}
			}
		} else {
			++stacktop;                 // Retain node on stack
			do {                        // Find lower rightmost key to pull up
				node = getNode(k->lson);
				i = node->count;
				k = node->keyI(i);
				push(node, i);
			} while (k->lson);
			node = pop(k, i);
			i--;
			k = node->keyI(i);
			// need only the offset and key
			int tlen = node->keyofs[-1-i] - node->keyofs[-i];
			KeyEntry* tkey = (KeyEntry*) new byte[tlen];
			memcpy(tkey, k, tlen);
			node->dirty = true;
			if (!--node->count && stacktop) {
				ndxFilePosT son = k->lson;
				freeNode(node);
				node = pop(k, i);              // Back up to poppa
				k->lson = son;                 // No son now
				node->dirty = true;
			}
			stacktop = ttop;
			node = pop(k, i);
			int lendiff = tlen - klen;
			/* Substitute tkey for key being deleted */
			while (lendiff +                      // length difference between keys
				   node->keyofs[-node->count] +   // key data
				   sizeof(ndxFilePosT) +          // rson
				   + node->count * sizeof(uint16) // keyofs's
					   > nNodeSize)
			{
				int ret;
				do {
					ret = node->split(this);
					find(curKey, curOffset);
					node = pop(k, i);
				} while (ret == -1);
			}
			tkey->lson = k->lson; // Preserve the original lson
			if (lendiff) {
				memmove((byte*)k + tlen, (byte*)k + klen,
						node->keyofs[-node->count] - node->keyofs[-1-i] + sizeof(ndxFilePosT));
				j = i + 1;
				uint16* w = node->keyofs - j;
				while (j <= node->count) {
					*w-- += lendiff;
					j++;
				}
			}
			memcpy(k, tkey, tlen);
			delete[] tkey;
			node->dirty = true;
			k = (KeyEntry*)((byte*)k + tlen);
			i++;
			if (i == node->count && !k->lson) {
				// If at end of node, up to next key
				while (i == node->count) {
					if (!stacktop) {
						n--;
						clearCurKey();
						return true;
					}
					node = pop(k, i);
				}
				push(node, i);
			} else {
				push(node, i);
				while (k->lson) {            // Get lowest left son, if any
					node = getNode(k->lson);
					k = &node->key0;
					push(node, 0);
				}
			}
		}
		// new current key
		IKey::copy(curKey, k->key);
		curOffset = k->offset;
		n--;
		return true;
	}

    /// Goto beginning of index for sequential scanning
	bool first() // throw(...)
	{
		if (!f) return false;
		stacktop = 0;
		Node* node = getNode(root);
		if (!node->count) return false;
		/* Find lowermost leftmost key in the index, setting the stack */
		KeyEntry* k;
		while (1) {
			push(node, 0);
			k = &node->key0;
			if (!k->lson) 
				break;
			node = getNode(k->lson);
		}
		IKey::copy(curKey, k->key);
		curOffset = k->offset;
		return true;
	}

    /// Goto end of index for reverse scanning
	bool last() // throw(...)
	{
		if (!f) return false;
		stacktop = 0;
		Node* node = getNode(root);
		if (!node->count) return false;
		// Find lowermost rightmost key in the index, setting the stack
		KeyEntry* k;
		int i;
		while (1) {
			k = node->keyI(i = node->count);
			if (!k->lson)
				break;
			push(node, i);
			node = getNode(k->lson);
		};
		k = node->keyI(--i);
		push(node, i);
		IKey::copy(curKey, k->key);
		curOffset = k->offset;
		return true;
	}

    /// Goto next key from the current. returns false at eof
	bool next() // throw(...)
	{
		if (!f || !stacktop) return false;

		KeyEntry* k;
		int       i;
		/* The state at return is that left by find(): The top of the stack is the
		 * node in which a key was last found and the offset is that of the key.  */
		Node* node = pop(k, i);
		if (++i > node->count) {
			assert(0);   // should never happen
			clearCurKey();
			return false;
		}
		k = node->keyI(i);
		while (k->lson) {            // While left son, descend to lowest one
			push(node, i);
			node = getNode(k->lson);
			k = &node->key0;
			i = 0;
		}
		// While at end of current node, back up a level
		while (i == node->count)
			if (stacktop)
				node = pop(k, i);
			else {
				clearCurKey();
				return false;
			}

		push(node, i);
		IKey::copy(curKey, k->key);
		curOffset = k->offset;
		return true;
	}

    /// Goto previous key. returns false at bof
	bool prev() // throw(...)
	{
		if (!f || !stacktop) return false;

		KeyEntry* k;
		int       i;
		Node* node = pop(k, i);
		while (k->lson) {
			push(node, i);
			node = getNode(k->lson);
			i = node->count;
			k = node->keyI(i);
		}
		while (i == 0)
			if (stacktop)
				node = pop(k, i);
			else  {
				clearCurKey();
				return false;
			}
		i--;
		k = node->keyI(i);
		push(node, i);
		IKey::copy(curKey, k->key);
		curOffset = k->offset;
		return true;
	}

    /// Returns true if duplicate keys are permitted
	bool dupsAllowed() const // noexcept // throw()
		{ return dups; }

    /// Debugging code to output the index tree
	void print(char* filename) // throw(...)
	{
		FILE* outf = fopen(filename, "w");
		_print(outf, root, 0);
		fclose(outf);
	}

protected:
	byte        major;        // Version number
	byte        minor;
	byte        hNdxPosSize;  // stored index file offset size
	byte        hDatPosSize;  // stored data file offset size
    uint16      hNodeSize;    // Stored nNodeSize for open() sanity check
	uint16      hMaxKeySize;  // Stored for sanity check

	ndxFilePosT root;         // File offset of the root node
	ndxFilePosT eof;          // File offset of the end of file
	ndxFilePosT freelist;	  // File offset of free node list
	int32       n;			  // File size (# keys, that is)

	bool        dups;         // index allows duplicate keys

	byte        filler[3];    // align stacktop

	/// Fields above stacktop are stored in the index header

	int         stacktop;            // Current stack top index
	StackFrame  stack[ndxMaxStack];  // Current state

	FileSystem::FileHandle f;        // Index file handle

	Node**      cache;     // The node cache
	int         cacheUsed; // number of used cache nodes
	int         nMaxCache; // max cache nodes

	int         ckeyno;    // number of ckey in the index; only maintained during pack
	void*       curKey;
	datFilePosT curOffset;

	int  	    nMaxKeySize;  // calculated

	void clearCurKey()
	{
		memcpy(curKey, IKey::emptyKey(), IKey::emptyKeySize());
		curOffset = 0;
	}

	/// reset the cache.  used by open() and create()
	void resetCache()
	{
		Node** c = cache;
		for (int i = 0; i < cacheUsed; i++)
			(*c++)->dirty = false;
		cacheUsed = 0;
	}

	/// seek to specified offset in index file
	void seek(const ndxFilePosT& offset) // throw(...)  // can throw io_error
	{
		FileSystemT::seek(f, offset);
	}

    /// Read header or node
	void read(const ndxFilePosT& offset, void* buffer, uint16 size) // throw(...)  // can throw io_error
	{
		FileSystemT::seek(f, offset);
		FileSystemT::read(f, buffer, size);
	}

	/// Write header or node
    void write(const ndxFilePosT& offset, void* buffer, uint16 size) // throw(...)  // can throw io_error
	{
		FileSystemT::seek(f, offset);
		FileSystemT::write(f, buffer, size);
	}

     /// Get a specific node
	Node* getNode(const ndxFilePosT& offset) // throw(...) // can throw io_error(), called by almost everything
	{
		Node* node = 0;
		int i = cacheUsed;
		if (offset) {
			Node** c = cache;
			for (i = 0; i < cacheUsed; i++) { // It may be in the cache
				node = *c++;
				if (node->offset == offset)
					break;
			}
		}
		if (i == cacheUsed) {
			if (cacheUsed < nMaxCache)       // if not using all the cache slots,
				node = cache[i = cacheUsed++];  //    use another slot
			else {
				node = cache[i = nMaxCache-1];  // reuse oldest node in cache
				if (node->dirty) {
					write(node->offset, node, nNodeSize);
					node->dirty = false;
				}
			}
			if (offset) {
				read(offset, node, nNodeSize);
				node->offset = offset;
			}
		}
		if (i) {  // bubble up cache slots
			memmove(cache + 1, cache, i * sizeof(Node*));
			cache[0] = node;
		}
		return node;
	}

    // Get an empty node (maybe from freelist)
	Node* newNode() // throw(...) // can throw io_error(), called by insert(), create()
	{
		Node* node = getNode(0); // New slot in the cache
		if (freelist) {          // If we can use an old node
			node->offset = freelist;
			read(freelist, &freelist, sizeof(freelist));
		} else {                      // extend the file
			write(node->offset = eof, node, nNodeSize);
			eof += nNodeSize;
		}
		node->count = 0;
		node->lson = 0;
		node->dirty = true;
		return node;
	}

	// add a node to the free list on disk
	void freeNode(Node* node) // noexcept
	{
		write(node->offset, &freelist, sizeof(ndxFilePosT));
		freelist = node->offset;
		node->dirty = false;
		Node** c = cache;
		int i;
		for (i = 0; *c != node; i++, c++)
			;
		cacheUsed--;
		memmove(c, c + 1, (cacheUsed - i) * sizeof(Node*));
		cache[cacheUsed] = node;
	}

	// put a node and key index onto the stack
	void push(Node* node, int i) // throw(...)
	{
		StackFrame* stk = &stack[stacktop++];
		if (stacktop > ndxMaxStack) {
			char message[1024];
			sprintf(message, "Index stack overflow in file %s", FileSystemT::getName(f));
			throw runtime_error(message);
		}
		stk->offset = node->offset;
		stk->i = i;
	}

	// get top node and key offset the stack and pop it
	Node* pop(KeyEntry* &k, int &i) // throw(...)
	{
		if (!stacktop) {
			char message[1024];
			sprintf(message, "Stack underflow: no current key. File: %s", FileSystemT::getName(f));
			throw logic_error(message);
		}
		StackFrame* stk = &stack[--stacktop];
		Node* node = getNode(stk->offset);
		k = node->keyI(stk->i);
		i = stk->i;
		return node;
	}

	// get the top node and key off the stack
	Node* top(KeyEntry* &k, int &i) // noexcept
	{
		Node* node = pop(k, i);
		stacktop++;
		return node;
	}

	// Inner insert
	int	_insert(int size, const ndxFilePosT& root) // throw(...)
	{
		Node* node = getNode(root);
		KeyEntry* k;
		int i = 0;
		int j = node->count;
		while (j > i) {
			int m = (i + j) / 2;
			k = node->keyI(m);
			int cmp = IKey::compare(curKey, k->key);
			if (cmp < 0)
				j = m;
			else if (cmp > 0)
				i = m + 1;
			else if (!dups) {
				push(node, m);
				return false;
			} else if (curOffset < k->offset)
				j = m;
			else if (curOffset > k->offset)
				i = m + 1;
			else {
				push(node, m);
				return false;
			}
		}
		k = node->keyI(i);
		if (k->lson) {                /* Recurse */
			push(node, i);
			return _insert(size, k->lson);
		}

		if (size + sizeof(uint16) +         // new KeyEntry & it's keyofs
			node->keyofs[-node->count] +    // node->count & current key data
			sizeof(ndxFilePosT) +           // rson
			node->count * sizeof(uint16) >  // current keyofs's 
			nNodeSize) {
			// Node full, so split
			node->split(this);
			return -1;
		}

		// make room for key in the node
		memmove((byte*)k + size, k,
			node->keyofs[-node->count] - ((byte*)k - (byte*)node) + sizeof(ndxFilePosT));

		// adjust the keyofs's
		j = ++node->count;
		uint16* w = node->keyofs - j;
		while (j > i) {
			*w++ = w[1] + size;
			j--;
		}
		memcpy(k->key, curKey, size - FIELDOFFSET(KeyEntry, key));
//		KeyEntry* q = (KeyEntry*)((byte*)k + size);
		k->lson = 0;
		k->offset = curOffset;
		node->dirty = true;
		push(node, i);
		return true;
	}

	// Inner key search routine
	bool _find(const void* key, const ndxFilePosT& root) // throw(...)
	{
		Node* node = getNode(root);
		KeyEntry* k = &node->key0;
		int i = 0;
		int j = node->count;
		while (j > i) {
			int m = (i + j) / 2;
			k = node->keyI(m);
			int cmp = IKey::compare(key, k->key);
			if (cmp < 0)
				j = m;
			else if (cmp > 0)
				i = m + 1;
			else {
				push(node, m);
				IKey::copy(curKey, k->key);
				curOffset = k->offset;
				return true;
			}
		}
		k = node->keyI(i);
		// Ran into larger key or at end of this level
		push(node, i);
		bool rcd = false;
		if (k->lson)                 // More keys on right -- recurse
			rcd = _find(key, k->lson);
		return rcd;
	}

 // Inner key search routine
	bool _find(const void* key, const datFilePosT& offset, const ndxFilePosT& root) // throw(...)
	{
		Node* node = getNode(root);
		KeyEntry* k = &node->key0;
		int i = 0;
		int j = node->count;
		while (j > i) {
			int m = (i + j) / 2;
			k = node->keyI(m);
			int cmp = IKey::compare(key, k->key);
			if (cmp < 0)
				j = m;
			else if (cmp > 0)
				i = m + 1;
			else if (offset < k->offset)
				j = m;
			else if (offset > k->offset)
				i = m + 1;
			else {
				push(node, m);
				IKey::copy(curKey, k->key);
				curOffset = k->offset;
				return true;
			}
		}
		k = node->keyI(i);
		// Ran into larger key or at end of this level
		push(node, i);
		bool rcd = false;
		if (k->lson)                  // More keys on right -- recurse
			rcd = _find(key, offset, k->lson);
		return rcd;
	}

	void _findNext(void) // throw(...)
	{
		KeyEntry* k;
		int i;
		Node* node = pop(k, i);
		while (i == node->count)
			if (stacktop)
				node = pop(k, i);
			else {
				clearCurKey();
				return;
			}
		stacktop++;
		IKey::copy(curKey, k->key);
		curOffset = k->offset;
	}

	// debugging helper
	bool _verify(Node* node)
	{
		uint16 ofs = FIELDOFFSET(Node, key0);
		int i;
		for (i = 0; i < node->count; i++) {
			if (node->keyofs[-i] != ofs)
				return false;
			KeyEntry* k = (KeyEntry*)((byte*)node + ofs);
			ofs += k->size();
		}
		return node->keyofs[-i] == ofs;
	}

	void _print(FILE* outf, const ndxFilePosT& offset, int level) // throw(...)
	{
		for (int i = 0; i < level * 8; i++)
			fputc(' ', outf);
		fprintf(outf, "---------\n");
		Node* node = getNode(offset);
		for (int i = 0; i <= node->count; i++) {
			KeyEntry* k = node->keyI(i);
			if (k->lson) {
				ndxFilePosT child(k->lson);
				_print(outf, child, level+1);
				if (node->offset != offset) {
					node = getNode(offset);
					k = node->keyI(i);
				}
			}
			if (i < node->count) {
				for (int j = 0; j < level * 8; j++)
					fputc(' ', outf);
				fprintf(outf, "%s, %u\n", IKey::toString(k->key), k->offset);
			}
		}
	}   
};


typedef IndexT<>          Index;
typedef IndexT<IKeyUTF16> UniIndex;


#pragma pack(pop)

} // namespace nub

#endif // __NUB_INDEX_H__
