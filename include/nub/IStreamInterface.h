#include <ObjIdl.h>
#include <istream>

namespace nub {

using namespace std;

class IStreamInterface : public IStream
{
public:
    IStreamInterface(istream* is) : m_is(is) {}

    ULONG STDMETHODCALLTYPE Release() { 
        if (--refCount == 0)
            delete m_is;
        return refCount;
    }

    HRESULT STDMETHODCALLTYPE Read(void *pv, ULONG cb, ULONG *pcbRead) {
        m_is->read((char*)pv, cb);
        if (m_is->fail()) {
            if (pcbRead) *pcbRead = 0;
            return S_FALSE;
        }
        if (pcbRead) *pcbRead = cb;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition) {
        _int64 off = *(_int64*)&dlibMove;
        if (dwOrigin == STREAM_SEEK_SET)
            m_is->seekg(off);
        else if (dwOrigin == STREAM_SEEK_CUR && off != 0)
            m_is->seekg(off, ios_base::cur);
        else
            m_is->seekg(off, ios_base::end);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Stat(STATSTG *p, DWORD grfStatFlag) {
        memset(p, 0, sizeof(STATSTG));
        p->type = STGTY_STREAM;
        _int64 pos = m_is->tellg();
        m_is->seekg(0, ios_base::end);
        *(_int64*)(&p->cbSize) = m_is->tellg(); 
        m_is->seekg(pos);
        if (grfStatFlag != STATFLAG_NONAME)
            wcscpy(p->pwcsName, L"nub::IStreamInterface");
        return S_OK;
    }

    void Unimplemented(char* funcname) {
        MessageBoxA(0, funcname, "Unimplemeneted IStream function", MB_ICONEXCLAMATION);
        exit(1);
    }

    /* [local] */ HRESULT STDMETHODCALLTYPE Write( 
        /* [annotation] */ 
        __in_bcount(cb)  const void *pv,
        /* [in] */ ULONG cb,
        /* [annotation] */ 
        __out_opt  ULONG *pcbWritten)
    { 
        Unimplemented("Write"); 
        return STG_E_ACCESSDENIED;
    }

    HRESULT STDMETHODCALLTYPE SetSize( 
        /* [in] */ ULARGE_INTEGER libNewSize) 
    { 
        Unimplemented("SetSize");
        return STG_E_INVALIDFUNCTION;
    }
        
    /* [local] */ HRESULT STDMETHODCALLTYPE CopyTo( 
        /* [unique][in] */ IStream *pstm,
        /* [in] */ ULARGE_INTEGER cb,
        /* [annotation] */ 
        __out_opt  ULARGE_INTEGER *pcbRead,
        /* [annotation] */ 
        __out_opt  ULARGE_INTEGER *pcbWritten)
    { 
        Unimplemented("CopyTo");
        return STG_E_INVALIDFUNCTION;
    }
        
    HRESULT STDMETHODCALLTYPE Commit( 
        /* [in] */ DWORD grfCommitFlags)
    {
        Unimplemented("Commit");
        return STG_E_INVALIDFUNCTION;
    }

    HRESULT STDMETHODCALLTYPE Revert( void)
    { 
        Unimplemented("Revert");
        return STG_E_INVALIDFUNCTION;
    }

    HRESULT STDMETHODCALLTYPE LockRegion( 
        /* [in] */ ULARGE_INTEGER libOffset,
        /* [in] */ ULARGE_INTEGER cb,
        /* [in] */ DWORD dwLockType)
    { 
        Unimplemented("LockRegion");
        return STG_E_INVALIDFUNCTION;
    }

    HRESULT STDMETHODCALLTYPE UnlockRegion( 
        /* [in] */ ULARGE_INTEGER libOffset,
        /* [in] */ ULARGE_INTEGER cb,
        /* [in] */ DWORD dwLockType)
    {
        Unimplemented("UnlockRegion");
        return STG_E_INVALIDFUNCTION;
    }
        
    HRESULT STDMETHODCALLTYPE Clone( 
        /* [out] */ __RPC__deref_out_opt IStream **ppstm)
    {
        Unimplemented("Clone");
        return STG_E_INVALIDFUNCTION;
    }    

    HRESULT STDMETHODCALLTYPE QueryInterface( 
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject)
    {
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef( void) 
        { return ++refCount; }

protected:
    std::istream* m_is;
    ULONG refCount;
};

} // namespace nub
