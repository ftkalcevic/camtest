// CamViewCommand.cpp : the regular command that shows toolpaths and receives picks.

#include "stdafx.h"
#include "camtest.h"
#include "CamViewCommand.h"
#include "CamTabCommand.h"
#include "PocHelpers.h"

namespace
{
    // Draws points as one polyline. The sample add-ons pass 1-based float arrays.
    void DrawPolyline(IADAddOnCanvasDisplay* pCanvas, __int64 segment, const std::vector<float>& points)
    {
        if (points.size() < 6)
            return;

        SAFEARRAY* pArray = SafeArrayCreateVector(VT_R4, 1, static_cast<ULONG>(points.size()));
        if (pArray == nullptr)
            return;

        float* pData = nullptr;
        if (SUCCEEDED(SafeArrayAccessData(pArray, reinterpret_cast<void**>(&pData))))
        {
            memcpy(pData, points.data(), points.size() * sizeof(float));
            SafeArrayUnaccessData(pArray);

            __int64 result = 0;
            const HRESULT hr = pCanvas->DrawPolyline(segment, &pArray, &result);
            if (FAILED(hr))
                PocLog(L"[view] DrawPolyline failed hr=0x%08X", static_cast<unsigned>(hr));
        }
        SafeArrayDestroy(pArray);
    }

    __int64 AddSegment(IADAddOnCanvasDisplay* pCanvas, const wchar_t* name,
                       long red, long green, long blue, bool foreground)
    {
        __int64 segment = 0;
        _bstr_t segmentName(name);
        pCanvas->AddSubSegment(0, segmentName, &segment);
        pCanvas->SetSegmentColor(segment, red, green, blue, 255);
        pCanvas->SetLineWeight(segment, 2.0f);
        pCanvas->ToggleForegroundRendering(segment, foreground ? VARIANT_TRUE : VARIANT_FALSE);
        return segment;
    }
}

CCamViewCommand::CCamViewCommand(CCamTabCommand* pTab)
    : CPocCommandBase(L"view")
    , m_pTab(pTab)
    , m_drawnRevision(-1)
    , m_reportedRender(false)
    , m_renderCalls(0)
    , m_redraws(0)
    , m_lastRenderLog(0)
{
    m_pTab->AddRef();
}

CCamViewCommand::~CCamViewCommand()
{
    if (m_pTab != nullptr)
        m_pTab->Release();
}

void CCamViewCommand::Refresh()
{
    if (m_site != nullptr)
        m_site->InvalidateCanvas();
}

STDMETHODIMP CCamViewCommand::OnComplete()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    PocLog(L"[view] OnComplete");
    Refresh();
    return S_OK;
}

// These four log input to explain why the view sometimes ends with no tab change.
STDMETHODIMP CCamViewCommand::OnClick(long screenX, long screenY, enum ADDONMouseButtons buttons,
                                      VARIANT_BOOL* pHandled)
{
    PocLog(L"[input] click buttons=%d at (%ld, %ld)", static_cast<int>(buttons), screenX, screenY);
    return CPocCommandBase::OnClick(screenX, screenY, buttons, pHandled);
}

STDMETHODIMP CCamViewCommand::OnDoubleClick(long screenX, long screenY, VARIANT_BOOL* pHandled)
{
    PocLog(L"[input] double click at (%ld, %ld)", screenX, screenY);
    return CPocCommandBase::OnDoubleClick(screenX, screenY, pHandled);
}

STDMETHODIMP CCamViewCommand::OnMouseDown(long screenX, long screenY, enum ADDONMouseButtons buttons,
                                          VARIANT_BOOL* pHandled)
{
    PocLog(L"[input] mouse down buttons=%d at (%ld, %ld)", static_cast<int>(buttons), screenX, screenY);
    return CPocCommandBase::OnMouseDown(screenX, screenY, buttons, pHandled);
}

STDMETHODIMP CCamViewCommand::OnKeyDown(long keycode, VARIANT_BOOL* pHandled)
{
    PocLog(L"[input] key down %ld", keycode);
    return CPocCommandBase::OnKeyDown(keycode, pHandled);
}

STDMETHODIMP CCamViewCommand::OnSelectionChange()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    if (m_pTab != nullptr)
        m_pTab->HandleSelection();
    return S_OK;
}

STDMETHODIMP CCamViewCommand::OnTerminate()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    PocLog(L"[view] OnTerminate - toolpaths are no longer drawn (renders=%ld redraws=%ld)",
           m_renderCalls, m_redraws);

    if (m_pTab != nullptr)
    {
        CCamTabCommand* pTab = m_pTab;
        m_pTab = nullptr;
        pTab->OnViewTerminated(this);
        pTab->Release();
    }
    m_site = nullptr;
    return S_OK;
}

STDMETHODIMP CCamViewCommand::On3DRender()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    if (m_site == nullptr || m_pTab == nullptr)
        return S_OK;

    const CamScene& scene = m_pTab->GetScene();
    const bool changed = (m_drawnRevision != scene.revision);
    const bool dirty = changed || scene.redrawEveryRender;

    ++m_renderCalls;
    if (dirty)
        ++m_redraws;

    const ULONGLONG now = GetTickCount64();
    if (now - m_lastRenderLog >= 500)
    {
        PocLog(L"[view] On3DRender renders=%ld redraws=%ld foreground=%d redrawEveryRender=%d", m_renderCalls,
               m_redraws, scene.foreground ? 1 : 0, scene.redrawEveryRender ? 1 : 0);
        m_lastRenderLog = now;
    }

    IUnknown* pContext = nullptr;
    const HRESULT hr = m_site->Begin3DDisplay(dirty ? VARIANT_TRUE : VARIANT_FALSE, &pContext);

    if (!m_reportedRender)
    {
        VARIANT_BOOL legacy = VARIANT_FALSE;
        m_site->LegacyRenderingEngine(&legacy);
        PocLog(L"[view] first On3DRender: Begin3DDisplay hr=0x%08X, legacy renderer=%d",
               static_cast<unsigned>(hr), legacy != VARIANT_FALSE ? 1 : 0);
        m_reportedRender = true;
    }

    if (FAILED(hr))
        return S_OK;

    if (pContext != nullptr)
    {
        IADAddOnCanvasDisplayPtr canvas = PocQI<IADAddOnCanvasDisplayPtr>(pContext);
        pContext->Release();

        if (canvas != nullptr && dirty)
        {
            const ULONGLONG start = GetTickCount64();
            PocGuard(L"draw scene", [&] { DrawScene(canvas); });
            m_drawnRevision = scene.revision;
            if (changed)
                PocLog(L"[view] drew scene revision %d in %llu ms", m_drawnRevision, GetTickCount64() - start);
        }
    }

    m_site->End3DDisplay();
    return S_OK;
}

void CCamViewCommand::DrawScene(IADAddOnCanvasDisplay* pCanvas)
{
    const CamScene& scene = m_pTab->GetScene();

    // Reference square at the origin: 10 units per side, plus one diagonal.
    // At 10 cm per side it proves the canvas uses centimetres.
    {
        const __int64 segment = AddSegment(pCanvas, L"Reference", 0, 200, 255, scene.foreground);
        const std::vector<float> square =
        {
            0, 0, 0,    10, 0, 0,    10, 10, 0,    0, 10, 0,    0, 0, 0,    10, 10, 0
        };
        DrawPolyline(pCanvas, segment, square);
    }

    // Large polyline for the performance test: a zigzag across the square.
    if (scene.bigPolylinePoints > 0)
    {
        const __int64 segment = AddSegment(pCanvas, L"Large toolpath", 255, 140, 0, scene.foreground);
        std::vector<float> points;
        points.reserve(static_cast<size_t>(scene.bigPolylinePoints) * 3);
        for (int i = 0; i < scene.bigPolylinePoints; ++i)
        {
            points.push_back(10.0f * static_cast<float>(i) / static_cast<float>(scene.bigPolylinePoints));
            points.push_back((i % 2 == 0) ? 0.0f : 10.0f);
            points.push_back(1.0f);
        }
        DrawPolyline(pCanvas, segment, points);
    }

    // Bounding box of the part read by the stock test.
    if (scene.hasStock)
    {
        const __int64 segment = AddSegment(pCanvas, L"Stock", 0, 200, 0, scene.foreground);
        const float* lo = scene.stockMin;
        const float* hi = scene.stockMax;
        const std::vector<float> box =
        {
            lo[0], lo[1], lo[2],   hi[0], lo[1], lo[2],   hi[0], hi[1], lo[2],   lo[0], hi[1], lo[2],
            lo[0], lo[1], lo[2],   lo[0], lo[1], hi[2],   hi[0], lo[1], hi[2],   hi[0], hi[1], hi[2],
            lo[0], hi[1], hi[2],   lo[0], lo[1], hi[2],   lo[0], hi[1], hi[2],   lo[0], hi[1], lo[2],
            hi[0], hi[1], lo[2],   hi[0], hi[1], hi[2],   hi[0], lo[1], hi[2],   hi[0], lo[1], lo[2]
        };
        DrawPolyline(pCanvas, segment, box);
    }

    // Markers from picks. Red is the raw clicked point. Yellow is the centre
    // of the picked entity, moved by the occurrence's world transform.
    if (!scene.clickMarkers.empty())
    {
        const __int64 segment = AddSegment(pCanvas, L"Clicked points", 255, 0, 0, scene.foreground);
        for (const CamMarker& marker : scene.clickMarkers)
            pCanvas->DrawMarker(segment, marker.x, marker.y, marker.z, 20, MarkerType_MARKER_CROSSHAIR);
    }
    if (!scene.entityMarkers.empty())
    {
        const __int64 segment = AddSegment(pCanvas, L"Picked entities", 255, 255, 0, scene.foreground);
        for (const CamMarker& marker : scene.entityMarkers)
            pCanvas->DrawMarker(segment, marker.x, marker.y, marker.z, 20, MarkerType_MARKER_RECT);
    }
}
