// PocHelpers.cpp : small helpers shared by the API proof of concept.

#include "stdafx.h"
#include "PocHelpers.h"

PocKey PocKeyFromArray(SAFEARRAY* pArray)
{
    PocKey key;
    if (pArray == nullptr)
        return key;

    SafeArrayGetVartype(pArray, &key.vt);

    LONG lower = 0, upper = -1;
    SafeArrayGetLBound(pArray, 1, &lower);
    SafeArrayGetUBound(pArray, 1, &upper);

    const size_t bytes = static_cast<size_t>(upper - lower + 1) * SafeArrayGetElemsize(pArray);

    BYTE* pData = nullptr;
    if (bytes > 0 && SUCCEEDED(SafeArrayAccessData(pArray, reinterpret_cast<void**>(&pData))))
    {
        static const wchar_t digits[] = L"0123456789ABCDEF";
        key.hex.reserve(bytes * 2);
        for (size_t i = 0; i < bytes; ++i)
        {
            key.hex += digits[pData[i] >> 4];
            key.hex += digits[pData[i] & 0x0F];
        }
        SafeArrayUnaccessData(pArray);
    }
    return key;
}

PocKey PocTakeKey(SAFEARRAY* pArray)
{
    PocKey key = PocKeyFromArray(pArray);
    if (pArray != nullptr)
        SafeArrayDestroy(pArray);
    return key;
}

SAFEARRAY* PocArrayFromKey(const PocKey& key)
{
    if (key.IsEmpty())
        return nullptr;

    const size_t bytes = key.hex.size() / 2;

    // Create one element first to learn the element size for this type.
    SAFEARRAY* pProbe = SafeArrayCreateVector(key.vt, 0, 1);
    if (pProbe == nullptr)
        return nullptr;
    const UINT elementSize = SafeArrayGetElemsize(pProbe);
    SafeArrayDestroy(pProbe);

    if (elementSize == 0)
        return nullptr;

    SAFEARRAY* pArray = SafeArrayCreateVector(key.vt, 0, static_cast<ULONG>(bytes / elementSize));
    if (pArray == nullptr)
        return nullptr;

    BYTE* pData = nullptr;
    if (SUCCEEDED(SafeArrayAccessData(pArray, reinterpret_cast<void**>(&pData))))
    {
        const auto nibble = [](wchar_t c) -> BYTE
        {
            return static_cast<BYTE>(c >= L'A' ? c - L'A' + 10 : c - L'0');
        };
        for (size_t i = 0; i < bytes; ++i)
            pData[i] = static_cast<BYTE>((nibble(key.hex[i * 2]) << 4) | nibble(key.hex[i * 2 + 1]));
        SafeArrayUnaccessData(pArray);
    }
    return pArray;
}

const wchar_t* PocUnitsName(ADUnits units)
{
    switch (units)
    {
    case ADUnits_AD_MILLIMETERS: return L"mm";
    case ADUnits_AD_CENTIMETERS: return L"cm";
    case ADUnits_AD_METERS:      return L"m";
    case ADUnits_AD_INCHES:      return L"in";
    case ADUnits_AD_FEET:        return L"ft";
    case ADUnits_AD_FEET_INCHES: return L"ft-in";
    default:                     return L"other";
    }
}

const wchar_t* PocGeometryName(ADGeometryType type)
{
    switch (type)
    {
    case ADGeometryType_AD_LINE:           return L"line";
    case ADGeometryType_AD_CIRCLE:         return L"circle";
    case ADGeometryType_AD_ELLIPSE:        return L"ellipse";
    case ADGeometryType_AD_BSPLINE:        return L"bspline";
    case ADGeometryType_AD_CIRCULAR_ARC:   return L"arc";
    case ADGeometryType_AD_ELLIPTICAL_ARC: return L"elliptical arc";
    case ADGeometryType_AD_PLANE:          return L"plane";
    case ADGeometryType_AD_CYLINDER:       return L"cylinder";
    case ADGeometryType_AD_CONE:           return L"cone";
    case ADGeometryType_AD_SPHERE:         return L"sphere";
    case ADGeometryType_AD_TORUS:          return L"torus";
    case ADGeometryType_AD_BSURF:          return L"bspline surface";
    default:                               return L"other";
    }
}

namespace
{
    // Describes one modified item. The event's element type isn't documented,
    // so try the object types the CAM dependency tracking cares about.
    std::wstring DescribeItem(IUnknown* pItem)
    {
        std::wstring text = L"unknown object";
        PocGuard(L"describe item", [&]
        {
            if (IADOccurrencePtr occurrence = PocQI<IADOccurrencePtr>(pItem))
                text = std::wstring(L"occurrence ") + static_cast<const wchar_t*>(occurrence->GetName());
            else if (IADSessionPtr session = PocQI<IADSessionPtr>(pItem))
                text = std::wstring(L"session ") + static_cast<const wchar_t*>(session->GetName());
            else if (PocQI<IADFacePtr>(pItem) != nullptr)
                text = L"face";
            else if (PocQI<IADEdgePtr>(pItem) != nullptr)
                text = L"edge";
            else if (PocQI<IADPartFeaturePtr>(pItem) != nullptr)
                text = L"part feature";
        });
        return text;
    }
}

void PocLogEventArray(const wchar_t* label, SAFEARRAY* pArray)
{
    if (pArray == nullptr)
    {
        PocLog(L"  %s: null", label);
        return;
    }

    VARTYPE vt = VT_EMPTY;
    SafeArrayGetVartype(pArray, &vt);

    LONG lower = 0, upper = -1;
    SafeArrayGetLBound(pArray, 1, &lower);
    SafeArrayGetUBound(pArray, 1, &upper);

    PocLog(L"  %s: vt=%d count=%ld", label, static_cast<int>(vt), upper - lower + 1);

    const LONG last = (upper - lower >= 20) ? lower + 19 : upper;
    for (LONG i = lower; i <= last; ++i)
    {
        if (vt == VT_I4 || vt == VT_INT)
        {
            // Alibre sends change types as VT_INT.
            int value = 0;
            SafeArrayGetElement(pArray, &i, &value);
            PocLog(L"    [%ld] change type %d", i, value);
        }
        else if (vt == VT_DISPATCH || vt == VT_UNKNOWN)
        {
            IUnknown* pItem = nullptr;
            const HRESULT hr = SafeArrayGetElement(pArray, &i, &pItem);
            if (SUCCEEDED(hr) && pItem != nullptr)
            {
                PocLog(L"    [%ld] %s", i, DescribeItem(pItem).c_str());
                pItem->Release();
            }
            else
            {
                PocLog(L"    [%ld] no object (hr=0x%08X)", i, static_cast<unsigned>(hr));
            }
        }
        else if (vt == VT_VARIANT)
        {
            _variant_t value;
            if (SUCCEEDED(SafeArrayGetElement(pArray, &i, &value)))
            {
                if (value.vt == VT_DISPATCH || value.vt == VT_UNKNOWN)
                    PocLog(L"    [%ld] %s", i, DescribeItem(value.punkVal).c_str());
                else if (value.vt == VT_I4)
                    PocLog(L"    [%ld] change type %ld", i, value.lVal);
                else
                    PocLog(L"    [%ld] variant vt=%d", i, static_cast<int>(value.vt));
            }
        }
    }
}

bool PocTransformPoint(IADTransformation* pTransform, double in[3], double out[3])
{
    if (pTransform == nullptr)
        return false;

    SAFEARRAY* pArray = pTransform->Array();
    if (pArray == nullptr)
        return false;

    bool ok = false;
    double* m = nullptr;
    if (SUCCEEDED(SafeArrayAccessData(pArray, reinterpret_cast<void**>(&m))))
    {
        // The documented layout is by rows, with the axes in the first three.
        out[0] = in[0] * m[0] + in[1] * m[4] + in[2] * m[8]  + m[12];
        out[1] = in[0] * m[1] + in[1] * m[5] + in[2] * m[9]  + m[13];
        out[2] = in[0] * m[2] + in[1] * m[6] + in[2] * m[10] + m[14];
        ok = true;
        SafeArrayUnaccessData(pArray);
    }
    SafeArrayDestroy(pArray);
    return ok;
}
