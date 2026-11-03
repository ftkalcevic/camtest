// PocCommandBase.cpp : default IAlibreAddOnCommand behaviour for the POC commands.

#include "stdafx.h"
#include "PocCommandBase.h"
#include "PocHelpers.h"

namespace
{
    HRESULT NotHandled(VARIANT_BOOL* pHandled)
    {
        if (pHandled == nullptr)
            return E_POINTER;
        *pHandled = VARIANT_FALSE;
        return S_OK;
    }
}

CPocCommandBase::CPocCommandBase(const wchar_t* name)
    : m_name(name)
    , m_refCount(0)
{
}

CPocCommandBase::~CPocCommandBase()
{
}

/////////////////////////////////////////////////////////////////////////////
// IUnknown

STDMETHODIMP CPocCommandBase::QueryInterface(REFIID riid, void** ppObj)
{
    if (ppObj == nullptr)
        return E_POINTER;

    if (riid == IID_IUnknown || riid == IID_IDispatch || riid == __uuidof(IAlibreAddOnCommand))
    {
        *ppObj = static_cast<IAlibreAddOnCommand*>(this);
        AddRef();
        return S_OK;
    }

    *ppObj = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CPocCommandBase::AddRef()
{
    return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
}

STDMETHODIMP_(ULONG) CPocCommandBase::Release()
{
    long count = InterlockedDecrement(&m_refCount);
    if (count == 0)
        delete this;
    return static_cast<ULONG>(count);
}

/////////////////////////////////////////////////////////////////////////////
// IDispatch

STDMETHODIMP CPocCommandBase::GetTypeInfoCount(UINT* pctinfo)
{
    if (pctinfo == nullptr)
        return E_POINTER;
    *pctinfo = 0;
    return S_OK;
}

STDMETHODIMP CPocCommandBase::GetTypeInfo(UINT, LCID, ITypeInfo** ppTInfo)
{
    if (ppTInfo != nullptr)
        *ppTInfo = nullptr;
    return E_NOTIMPL;
}

STDMETHODIMP CPocCommandBase::GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*)
{
    return E_NOTIMPL;
}

STDMETHODIMP CPocCommandBase::Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*,
                                     EXCEPINFO*, UINT*)
{
    return E_NOTIMPL;
}

/////////////////////////////////////////////////////////////////////////////
// IAlibreAddOnCommand

STDMETHODIMP CPocCommandBase::putref_CommandSite(struct IADAddOnCommandSite* pSite)
{
    PocLog(L"[%s] putref_CommandSite site=%s", m_name, pSite != nullptr ? L"set" : L"null");
    m_site = pSite;
    return S_OK;
}

STDMETHODIMP CPocCommandBase::get_CommandSite(struct IADAddOnCommandSite** ppSite)
{
    if (ppSite == nullptr)
        return E_POINTER;
    *ppSite = m_site;
    if (*ppSite != nullptr)
        (*ppSite)->AddRef();
    return S_OK;
}

STDMETHODIMP CPocCommandBase::IsTwoWayToggle(VARIANT_BOOL* pValue)
{
    if (pValue == nullptr)
        return E_POINTER;
    *pValue = VARIANT_FALSE;
    return S_OK;
}

STDMETHODIMP CPocCommandBase::AddTab(VARIANT_BOOL* pValue)
{
    if (pValue == nullptr)
        return E_POINTER;
    *pValue = VARIANT_FALSE;
    return S_OK;
}

STDMETHODIMP CPocCommandBase::OnShowUI(__int64)
{
    return S_OK;
}

STDMETHODIMP CPocCommandBase::OnRender(long, long, long, long, long)
{
    return S_OK;
}

STDMETHODIMP CPocCommandBase::OnClick(long, long, enum ADDONMouseButtons, VARIANT_BOOL* pHandled)
{
    return NotHandled(pHandled);
}

STDMETHODIMP CPocCommandBase::OnDoubleClick(long, long, VARIANT_BOOL* pHandled)
{
    return NotHandled(pHandled);
}

STDMETHODIMP CPocCommandBase::OnMouseDown(long, long, enum ADDONMouseButtons, VARIANT_BOOL* pHandled)
{
    return NotHandled(pHandled);
}

STDMETHODIMP CPocCommandBase::OnMouseMove(long, long, enum ADDONMouseButtons, VARIANT_BOOL* pHandled)
{
    return NotHandled(pHandled);
}

STDMETHODIMP CPocCommandBase::OnMouseUp(long, long, enum ADDONMouseButtons, VARIANT_BOOL* pHandled)
{
    return NotHandled(pHandled);
}

STDMETHODIMP CPocCommandBase::OnSelectionChange()
{
    return S_OK;
}

STDMETHODIMP CPocCommandBase::OnTerminate()
{
    PocLog(L"[%s] OnTerminate", m_name);
    return S_OK;
}

STDMETHODIMP CPocCommandBase::OnComplete()
{
    PocLog(L"[%s] OnComplete", m_name);
    return S_OK;
}

STDMETHODIMP CPocCommandBase::OnKeyDown(long, VARIANT_BOOL* pHandled)
{
    return NotHandled(pHandled);
}

STDMETHODIMP CPocCommandBase::OnKeyUp(long, VARIANT_BOOL* pHandled)
{
    return NotHandled(pHandled);
}

STDMETHODIMP CPocCommandBase::OnEscape(VARIANT_BOOL* pHandled)
{
    PocLog(L"[%s] OnEscape", m_name);
    return NotHandled(pHandled);
}

STDMETHODIMP CPocCommandBase::OnMouseWheel(double, VARIANT_BOOL* pHandled)
{
    return NotHandled(pHandled);
}

STDMETHODIMP CPocCommandBase::get_TabName(BSTR* pName)
{
    if (pName == nullptr)
        return E_POINTER;
    *pName = nullptr;
    return S_OK;
}

STDMETHODIMP CPocCommandBase::get_Extents(SAFEARRAY** ppExtents)
{
    if (ppExtents == nullptr)
        return E_POINTER;
    *ppExtents = nullptr;
    return S_OK;
}

STDMETHODIMP CPocCommandBase::On3DRender()
{
    return S_OK;
}
