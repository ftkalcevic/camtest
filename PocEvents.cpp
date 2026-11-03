// PocEvents.cpp : connects to Alibre's session events and logs them.
//
// No sample shows this from C++. EventManager lists two source interfaces, so
// this tries the vtable interface and then the dispatch one, and logs which
// connected. The dispatch IDs aren't documented, so that sink logs its raw
// arguments.

#include "stdafx.h"
#include "camtest.h"
#include "PocEvents.h"
#include "PocHelpers.h"

namespace
{
    std::wstring SessionName(IADSession* pSession)
    {
        std::wstring name = L"(none)";
        PocGuard(L"session name", [&]
        {
            if (pSession != nullptr)
                name = static_cast<const wchar_t*>(pSession->GetName());
        });
        return name;
    }

    /////////////////////////////////////////////////////////////////////////
    // Vtable sink

    class CVtableSink : public IADEventsCallback
    {
    public:
        CVtableSink() : m_refCount(1) {}

        STDMETHOD(QueryInterface)(REFIID riid, void** ppObj) override
        {
            if (ppObj == nullptr)
                return E_POINTER;
            if (riid == IID_IUnknown || riid == __uuidof(IADEventsCallback))
            {
                *ppObj = static_cast<IADEventsCallback*>(this);
                AddRef();
                return S_OK;
            }
            *ppObj = nullptr;
            return E_NOINTERFACE;
        }

        STDMETHOD_(ULONG, AddRef)() override
        {
            return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
        }

        STDMETHOD_(ULONG, Release)() override
        {
            const long count = InterlockedDecrement(&m_refCount);
            if (count == 0)
                delete this;
            return static_cast<ULONG>(count);
        }

        HRESULT __stdcall raw_OnInitialize() override
        {
            PocLog(L"[event] OnInitialize");
            return S_OK;
        }

        HRESULT __stdcall raw_OnTerminate() override
        {
            PocLog(L"[event] OnTerminate");
            return S_OK;
        }

        HRESULT __stdcall raw_OnSessionOpen(struct IADSession* pSession) override
        {
            PocLog(L"[event] OnSessionOpen %s", SessionName(pSession).c_str());
            return S_OK;
        }

        HRESULT __stdcall raw_OnSessionClose(struct IADSession* pSession) override
        {
            PocLog(L"[event] OnSessionClose %s", SessionName(pSession).c_str());
            return S_OK;
        }

        HRESULT __stdcall raw_OnSessionChange(struct IADSession* pSession, SAFEARRAY** pModifiedItems,
                                              SAFEARRAY** changeType) override
        {
            PocLog(L"[event] OnSessionChange %s", SessionName(pSession).c_str());
            PocLogEventArray(L"modified items", pModifiedItems != nullptr ? *pModifiedItems : nullptr);
            PocLogEventArray(L"change types", changeType != nullptr ? *changeType : nullptr);
            return S_OK;
        }

        HRESULT __stdcall raw_OnModelLoadComplete(struct IADSession* pSession) override
        {
            PocLog(L"[event] OnModelLoadComplete %s", SessionName(pSession).c_str());
            return S_OK;
        }

    private:
        long m_refCount;
    };

    /////////////////////////////////////////////////////////////////////////
    // Dispatch sink

    class CDispatchSink : public IDispatch
    {
    public:
        CDispatchSink() : m_refCount(1) {}

        STDMETHOD(QueryInterface)(REFIID riid, void** ppObj) override
        {
            if (ppObj == nullptr)
                return E_POINTER;
            if (riid == IID_IUnknown || riid == IID_IDispatch || riid == __uuidof(_IADEventsCallback))
            {
                *ppObj = static_cast<IDispatch*>(this);
                AddRef();
                return S_OK;
            }
            *ppObj = nullptr;
            return E_NOINTERFACE;
        }

        STDMETHOD_(ULONG, AddRef)() override
        {
            return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
        }

        STDMETHOD_(ULONG, Release)() override
        {
            const long count = InterlockedDecrement(&m_refCount);
            if (count == 0)
                delete this;
            return static_cast<ULONG>(count);
        }

        STDMETHOD(GetTypeInfoCount)(UINT* pctinfo) override
        {
            if (pctinfo == nullptr)
                return E_POINTER;
            *pctinfo = 0;
            return S_OK;
        }

        STDMETHOD(GetTypeInfo)(UINT, LCID, ITypeInfo** ppTInfo) override
        {
            if (ppTInfo != nullptr)
                *ppTInfo = nullptr;
            return E_NOTIMPL;
        }

        STDMETHOD(GetIDsOfNames)(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override
        {
            return E_NOTIMPL;
        }

        STDMETHOD(Invoke)(DISPID dispId, REFIID, LCID, WORD, DISPPARAMS* pParams, VARIANT*, EXCEPINFO*, UINT*) override
        {
            const UINT argCount = (pParams != nullptr) ? pParams->cArgs : 0;
            PocLog(L"[event] dispatch event id=%ld args=%u", static_cast<long>(dispId), argCount);

            for (UINT i = 0; i < argCount; ++i)
            {
                // Dispatch arguments arrive in reverse order.
                VARIANT& arg = pParams->rgvarg[argCount - 1 - i];
                const VARTYPE vt = arg.vt;

                if ((vt & VT_ARRAY) != 0)
                {
                    SAFEARRAY* pArray = ((vt & VT_BYREF) != 0)
                                            ? ((arg.pparray != nullptr) ? *arg.pparray : nullptr)
                                            : arg.parray;
                    PocLogEventArray(L"array argument", pArray);
                }
                else if (vt == VT_DISPATCH || vt == VT_UNKNOWN)
                {
                    IADSessionPtr session = PocQI<IADSessionPtr>(arg.punkVal);
                    PocLog(L"  argument %u: session %s", i, SessionName(session).c_str());
                }
                else
                {
                    PocLog(L"  argument %u: vt=%d", i, static_cast<int>(vt));
                }
            }
            return S_OK;
        }

    private:
        long m_refCount;
    };

    /////////////////////////////////////////////////////////////////////////

    IConnectionPointPtr g_connectionPoint;
    DWORD g_cookie = 0;

    bool TryConnect(IConnectionPointContainer* pContainer, REFIID iid, IUnknown* pSink, const wchar_t* label)
    {
        IConnectionPointPtr point;
        HRESULT hr = pContainer->FindConnectionPoint(iid, &point);
        PocLog(L"[event] FindConnectionPoint %s hr=0x%08X", label, static_cast<unsigned>(hr));
        if (FAILED(hr))
            return false;

        DWORD cookie = 0;
        hr = point->Advise(pSink, &cookie);
        PocLog(L"[event] Advise %s hr=0x%08X", label, static_cast<unsigned>(hr));
        if (FAILED(hr))
            return false;

        g_connectionPoint = point;
        g_cookie = cookie;
        return true;
    }

    // Keeps the event manager alive while connected.
    IUnknownPtr g_eventManager;
}

void PocEventsConnect(IADRoot* pRoot)
{
    PocGuard(L"connect events", [&]
    {
        g_eventManager = pRoot->GetEventManager();
        if (g_eventManager == nullptr)
        {
            PocLog(L"[event] GetEventManager returned nothing");
            return;
        }

        IConnectionPointContainerPtr container = PocQI<IConnectionPointContainerPtr>(g_eventManager);
        if (container == nullptr)
        {
            PocLog(L"[event] the event manager has no IConnectionPointContainer");
            return;
        }

        CVtableSink* pVtableSink = new CVtableSink();
        const bool vtableConnected = TryConnect(container, __uuidof(IADEventsCallback), pVtableSink, L"vtable");
        pVtableSink->Release();
        if (vtableConnected)
            return;

        CDispatchSink* pDispatchSink = new CDispatchSink();
        TryConnect(container, __uuidof(_IADEventsCallback), pDispatchSink, L"dispatch");
        pDispatchSink->Release();
    });
}

void PocEventsDisconnect()
{
    PocGuard(L"disconnect events", [&]
    {
        if (g_connectionPoint != nullptr)
            g_connectionPoint->Unadvise(g_cookie);
    });
    g_connectionPoint = nullptr;
    g_eventManager = nullptr;
    g_cookie = 0;
}
