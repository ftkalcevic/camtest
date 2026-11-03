// PocHelpers.h : small helpers shared by the API proof of concept.

#pragma once

#include "PocLog.h"

// QueryInterface into a smart pointer. Returns an empty pointer on failure
// rather than throwing, so "what type is this?" checks stay simple.
template <class TPtr>
TPtr PocQI(IUnknown* pUnknown)
{
    TPtr result;
    if (pUnknown != nullptr)
        pUnknown->QueryInterface(TPtr::GetIID(), reinterpret_cast<void**>(&result));
    return result;
}

// Runs one step and logs any failure, so one bad call never stops the rest of
// a test. Returns true when the step ran without an exception.
template <class TFunc>
bool PocGuard(const wchar_t* label, TFunc func)
{
    try
    {
        func();
        return true;
    }
    catch (const _com_error& error)
    {
        PocLog(L"  FAILED %s: hr=0x%08X", label, static_cast<unsigned>(error.Error()));
    }
    catch (...)
    {
        PocLog(L"  FAILED %s: unknown exception", label);
    }
    return false;
}

// A persistent key as saved to disk: the array's element type and its bytes.
struct PocKey
{
    VARTYPE vt = VT_EMPTY;
    std::wstring hex;

    bool IsEmpty() const { return hex.empty(); }
};

PocKey PocKeyFromArray(SAFEARRAY* pArray);          // does not destroy pArray
PocKey PocTakeKey(SAFEARRAY* pArray);               // destroys pArray
SAFEARRAY* PocArrayFromKey(const PocKey& key);      // caller destroys the result

const wchar_t* PocUnitsName(ADUnits units);
const wchar_t* PocGeometryName(ADGeometryType type);

// Logs a SAFEARRAY from an event: its type, size and up to 20 elements.
void PocLogEventArray(const wchar_t* label, SAFEARRAY* pArray);

// Reads the 16 doubles of a transform and applies them to a point.
bool PocTransformPoint(IADTransformation* pTransform, double in[3], double out[3]);
