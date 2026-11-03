// CamTabCommand.cpp : the two-way toggle command that owns the CAM tab.

#include "stdafx.h"
#include "camtest.h"
#include "AddOnInterface.h"
#include "CamTabCommand.h"
#include "CamViewCommand.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    const UINT_PTR kPollTimer = 1;
    const UINT kPollMilliseconds = 250;
    const int kBigPolylinePoints = 200000;

    enum
    {
        IDC_CAM_TREE = 14001,
        IDC_CAM_AUTO,
        IDC_CAM_FOREGROUND,
        IDC_CAM_REDRAW,
        IDC_CAM_SHOW,
        IDC_CAM_HIDE,
        IDC_CAM_BIG,
        IDC_CAM_FILTER,
        IDC_CAM_BIND,
        IDC_CAM_CLEAR,
        IDC_CAM_STOCK,
        IDC_CAM_OPENLOG,
        IDC_CAM_LOG
    };

    std::wstring KeysPath()
    {
        return PocLogFolder() + L"poc_keys.txt";
    }

    // One saved pick: occurrence key, target key, what it was, and its time stamp.
    struct SavedPick
    {
        PocKey occurrence;
        PocKey target;
        std::wstring kind;
        long timeStamp = 0;
        std::wstring occurrenceName;
    };

    void AppendSavedPick(const SavedPick& pick)
    {
        FILE* file = nullptr;
        if (_wfopen_s(&file, KeysPath().c_str(), L"a, ccs=UTF-8") != 0 || file == nullptr)
        {
            PocLog(L"  could not write %s", KeysPath().c_str());
            return;
        }
        fwprintf(file, L"%d\t%s\t%d\t%s\t%s\t%ld\t%s\n",
                 static_cast<int>(pick.occurrence.vt), pick.occurrence.hex.c_str(),
                 static_cast<int>(pick.target.vt), pick.target.hex.c_str(),
                 pick.kind.c_str(), pick.timeStamp, pick.occurrenceName.c_str());
        fclose(file);
    }

    std::vector<std::wstring> SplitTabs(const std::wstring& line)
    {
        std::vector<std::wstring> parts;
        size_t start = 0;
        for (;;)
        {
            const size_t tab = line.find(L'\t', start);
            if (tab == std::wstring::npos)
            {
                parts.push_back(line.substr(start));
                return parts;
            }
            parts.push_back(line.substr(start, tab - start));
            start = tab + 1;
        }
    }

    std::vector<SavedPick> ReadSavedPicks()
    {
        std::vector<SavedPick> picks;

        FILE* file = nullptr;
        if (_wfopen_s(&file, KeysPath().c_str(), L"r, ccs=UTF-8") != 0 || file == nullptr)
            return picks;

        wchar_t buffer[8192];
        while (fgetws(buffer, _countof(buffer), file) != nullptr)
        {
            std::wstring line(buffer);
            while (!line.empty() && (line.back() == L'\n' || line.back() == L'\r'))
                line.pop_back();

            const std::vector<std::wstring> parts = SplitTabs(line);
            if (parts.size() < 7)
                continue;

            SavedPick pick;
            pick.occurrence.vt = static_cast<VARTYPE>(_wtoi(parts[0].c_str()));
            pick.occurrence.hex = parts[1];
            pick.target.vt = static_cast<VARTYPE>(_wtoi(parts[2].c_str()));
            pick.target.hex = parts[3];
            pick.kind = parts[4];
            pick.timeStamp = _wtol(parts[5].c_str());
            pick.occurrenceName = parts[6];
            picks.push_back(pick);
        }
        fclose(file);
        return picks;
    }

    // Calls BindKeyToItem with a saved key.
    IDispatchPtr BindKey(IADSession* pSession, const PocKey& key, ADObjectType type)
    {
        IDispatchPtr result;
        SAFEARRAY* pArray = PocArrayFromKey(key);
        if (pArray == nullptr)
            return result;

        try
        {
            result = pSession->BindKeyToItem(&pArray, type);
        }
        catch (...)
        {
            SafeArrayDestroy(pArray);
            throw;
        }
        SafeArrayDestroy(pArray);
        return result;
    }

    void LogExtents(IADPoint* pLower, IADPoint* pUpper)
    {
        if (pLower != nullptr && pUpper != nullptr)
        {
            PocLog(L"    extents (%.5f, %.5f, %.5f) to (%.5f, %.5f, %.5f)",
                   pLower->GetX(), pLower->GetY(), pLower->GetZ(),
                   pUpper->GetX(), pUpper->GetY(), pUpper->GetZ());
        }
    }

    const wchar_t* DesignUnits(IADDesignSession* pDesign)
    {
        return (pDesign != nullptr)
                   ? PocUnitsName(pDesign->GetDesignProperties()->GetModelUnits())
                   : L"unknown";
    }
}

/////////////////////////////////////////////////////////////////////////////
// CCamTabCommand

CCamTabCommand::CCamTabCommand(CAddOnInterface* pAddOn, const std::wstring& sessionId)
    : CPocCommandBase(L"tab")
    , m_pAddOn(pAddOn)
    , m_sessionId(sessionId)
    , m_pWnd(nullptr)
    , m_pView(nullptr)
    , m_pickFilterIndex(0)
{
}

CCamTabCommand::~CCamTabCommand()
{
}

IADSessionPtr CCamTabCommand::GetSession() const
{
    return theApp.m_pRoot->GetSessions()->GetItem(_variant_t(m_sessionId.c_str()));
}

void CCamTabCommand::SceneChanged()
{
    ++m_scene.revision;
    if (m_pView != nullptr)
        m_pView->Refresh();
}

STDMETHODIMP CCamTabCommand::IsTwoWayToggle(VARIANT_BOOL* pValue)
{
    if (pValue == nullptr)
        return E_POINTER;
    *pValue = VARIANT_TRUE;
    return S_OK;
}

STDMETHODIMP CCamTabCommand::AddTab(VARIANT_BOOL* pValue)
{
    if (pValue == nullptr)
        return E_POINTER;
    *pValue = VARIANT_TRUE;
    return S_OK;
}

STDMETHODIMP CCamTabCommand::get_TabName(BSTR* pName)
{
    if (pName == nullptr)
        return E_POINTER;
    *pName = _bstr_t(L"CAM").Detach();
    return S_OK;
}

STDMETHODIMP CCamTabCommand::OnShowUI(__int64 hWnd)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    PocLog(L"[tab] OnShowUI hWnd=0x%llX", static_cast<unsigned long long>(hWnd));

    if (m_pWnd == nullptr)
    {
        m_pWnd = new CCamTabWnd(this);
        if (!m_pWnd->CreateInTab(reinterpret_cast<HWND>(hWnd)))
        {
            PocLog(L"[tab] could not create the tab window");

            // If creation got far enough, PostNcDestroy already deleted it.
            delete m_pWnd;
            m_pWnd = nullptr;
        }
    }
    return S_OK;
}

STDMETHODIMP CCamTabCommand::OnTerminate()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    PocLog(L"[tab] OnTerminate");

    StopView(L"tab closing");

    if (m_pWnd != nullptr)
    {
        CCamTabWnd* pWnd = m_pWnd;
        m_pWnd = nullptr;
        pWnd->DetachFromCommand();
        pWnd->DestroyWindow();
    }

    m_site = nullptr;

    // Drops the add-on's reference, so this may be the last one.
    AddRef();
    if (m_pAddOn != nullptr)
    {
        m_pAddOn->RemoveTabForSession(m_sessionId);
        m_pAddOn = nullptr;
    }
    Release();
    return S_OK;
}

void CCamTabCommand::CloseFromAddOn()
{
    m_pAddOn = nullptr;
    StopView(L"add-on unloading");

    if (m_pWnd != nullptr)
    {
        CCamTabWnd* pWnd = m_pWnd;
        m_pWnd = nullptr;
        pWnd->DetachFromCommand();
        pWnd->DestroyWindow();
    }
}

void CCamTabCommand::OnWindowGone()
{
    PocLog(L"[tab] window destroyed by Alibre");
    m_pWnd = nullptr;
}

void CCamTabCommand::StartView(const wchar_t* reason)
{
    if (m_pView != nullptr)
    {
        PocLog(L"[tab] view already active (%s)", reason);
        return;
    }
    if (m_site == nullptr)
    {
        PocLog(L"[tab] cannot start view, no command site (%s)", reason);
        return;
    }

    CCamViewCommand* pView = new CCamViewCommand(this);
    pView->AddRef();
    m_pView = pView;

    const HRESULT hr = m_site->InvokeNewCommand(pView);
    PocLog(L"[tab] InvokeNewCommand (%s) hr=0x%08X", reason, static_cast<unsigned>(hr));

    if (FAILED(hr))
    {
        m_pView = nullptr;
        pView->Release();
        return;
    }

    if (m_pView != nullptr)
        m_pView->Refresh();
}

void CCamTabCommand::StopView(const wchar_t* reason)
{
    if (m_pView == nullptr)
        return;

    PocLog(L"[tab] stopping view (%s)", reason);

    // Keep the command alive across Terminate, which calls OnViewTerminated.
    CCamViewCommand* pView = m_pView;
    pView->AddRef();

    pView->Refresh();

    if (pView->Site() != nullptr)
    {
        const HRESULT hr = pView->Site()->Terminate();
        PocLog(L"[tab] view Terminate hr=0x%08X", static_cast<unsigned>(hr));
    }
    pView->Release();
}

void CCamTabCommand::RefreshView()
{
    if (m_pView != nullptr)
    {
        PocLog(L"[tab] refresh: view active, redrawing");
        SceneChanged();
    }
    else
    {
        StartView(L"refresh button");
    }
}

HRESULT CCamTabCommand::CreateViewForRibbon(IAlibreAddOnCommand** ppCommand)
{
    if (m_pView != nullptr)
    {
        PocLog(L"[tab] ribbon Show Toolpaths: view already active");
        return S_OK;
    }

    CCamViewCommand* pView = new CCamViewCommand(this);
    pView->AddRef();
    m_pView = pView;

    PocLog(L"[tab] ribbon Show Toolpaths: returning a view command");
    return pView->QueryInterface(__uuidof(IAlibreAddOnCommand), reinterpret_cast<void**>(ppCommand));
}

void CCamTabCommand::OnViewTerminated(CCamViewCommand* pView)
{
    if (m_pView == pView)
    {
        m_pView = nullptr;
        pView->Release();
    }
}

/////////////////////////////////////////////////////////////////////////////
// Test: what does a pick return?

void CCamTabCommand::LogPickedTarget(IADOccurrence* pOccurrence, IDispatch* pTarget, PocKey& targetKey,
                                     std::wstring& kind, long& timeStamp)
{
    IADPointPtr lower, upper;
    bool hasExtents = false;

    if (IADFacePtr face = PocQI<IADFacePtr>(pTarget))
    {
        kind = L"face";
        targetKey = PocTakeKey(face->GetKey());
        timeStamp = face->GetTimeStamp();
        face->GetExtents(&lower, &upper);
        hasExtents = true;

        IADSurfacePtr surface = face->GetGeometry();
        const ADGeometryType type = surface->GetSurfaceType();
        PocLog(L"    FACE surface=%s timeStamp=%ld keyBytes=%zu", PocGeometryName(type), timeStamp,
               targetKey.hex.size() / 2);

        if (IADPlanePtr plane = PocQI<IADPlanePtr>(surface))
        {
            IADPointPtr root = plane->GetRootPoint();
            IADVectorPtr normal = plane->GetNormal();
            PocLog(L"    plane root (%.5f, %.5f, %.5f) normal (%.5f, %.5f, %.5f)",
                   root->GetX(), root->GetY(), root->GetZ(), normal->GetX(), normal->GetY(), normal->GetZ());
        }
        else if (IADCylinderPtr cylinder = PocQI<IADCylinderPtr>(surface))
        {
            IADPointPtr base = cylinder->GetBasePoint();
            IADVectorPtr axis = cylinder->GetAxis();
            PocLog(L"    cylinder base (%.5f, %.5f, %.5f) axis (%.5f, %.5f, %.5f) radius %.5f",
                   base->GetX(), base->GetY(), base->GetZ(), axis->GetX(), axis->GetY(), axis->GetZ(),
                   cylinder->GetRadius());
        }

        IADLoopsPtr loops = face->GetLoops();
        const long loopCount = loops->GetCount();
        for (long i = 0; i < loopCount; ++i)
        {
            IADLoopPtr loop = loops->GetItem(i);
            PocLog(L"    loop %ld: outer=%d edges=%ld", i, loop->GetIsOuter() != VARIANT_FALSE ? 1 : 0,
                   loop->GetEdges()->GetCount());
        }
    }
    else if (IADEdgePtr edge = PocQI<IADEdgePtr>(pTarget))
    {
        kind = L"edge";
        targetKey = PocTakeKey(edge->GetKey());
        timeStamp = edge->GetTimeStamp();
        edge->GetExtents(&lower, &upper);
        hasExtents = true;

        IADCurvePtr curve = edge->GetGeometry();
        PocLog(L"    EDGE curve=%s closed=%d timeStamp=%ld keyBytes=%zu", PocGeometryName(curve->GetCurveType()),
               curve->GetIsClosed() != VARIANT_FALSE ? 1 : 0, timeStamp, targetKey.hex.size() / 2);
    }
    else if (IADVertexPtr vertex = PocQI<IADVertexPtr>(pTarget))
    {
        kind = L"vertex";
        targetKey = PocTakeKey(vertex->GetKey());
        lower = vertex->GetPoint();
        upper = lower;
        hasExtents = true;
        PocLog(L"    VERTEX (%.5f, %.5f, %.5f) keyBytes=%zu", lower->GetX(), lower->GetY(), lower->GetZ(),
               targetKey.hex.size() / 2);
    }
    else
    {
        kind = L"other";
        PocLog(L"    target is not a face, edge, or vertex");
    }

    if (hasExtents)
    {
        LogExtents(lower, upper);

        // Part coordinates, moved by the occurrence's world transform. If the
        // yellow marker sits on the picked entity, the transform is right.
        double centre[3] =
        {
            (lower->GetX() + upper->GetX()) / 2.0,
            (lower->GetY() + upper->GetY()) / 2.0,
            (lower->GetZ() + upper->GetZ()) / 2.0
        };
        double world[3] = {};
        if (pOccurrence != nullptr && PocTransformPoint(pOccurrence->GetWorldTransform(), centre, world))
        {
            PocLog(L"    centre in part (%.5f, %.5f, %.5f), in assembly (%.5f, %.5f, %.5f)",
                   centre[0], centre[1], centre[2], world[0], world[1], world[2]);
            m_scene.entityMarkers.push_back({ static_cast<float>(world[0]), static_cast<float>(world[1]),
                                              static_cast<float>(world[2]) });
        }
    }
}

void CCamTabCommand::HandleSelection()
{
    PocGuard(L"selection", [&]
    {
        IADSessionPtr session = GetSession();

        IADPointPtr clicked;
        IObjectCollectorPtr selected = session->SelectedObjectsEx(&clicked);
        const long count = (selected != nullptr) ? selected->GetCount() : 0;

        PocLog(L"[pick] selection changed: %ld selected", count);
        if (count == 0)
            return;

        if (clicked != nullptr)
        {
            PocLog(L"  clicked point (%.5f, %.5f, %.5f)", clicked->GetX(), clicked->GetY(), clicked->GetZ());
            m_scene.clickMarkers.push_back({ static_cast<float>(clicked->GetX()), static_cast<float>(clicked->GetY()),
                                             static_cast<float>(clicked->GetZ()) });
        }

        PocGuard(L"assembly units", [&]
        {
            PocLog(L"  assembly model units=%s", DesignUnits(PocQI<IADDesignSessionPtr>(session)));
        });

        for (long i = 0; i < count; ++i)
        {
            IDispatchPtr item = selected->GetItem(_variant_t(i));
            IADTargetProxyPtr proxy = PocQI<IADTargetProxyPtr>(item);
            if (proxy == nullptr)
            {
                PocLog(L"  [%ld] not a target proxy", i);
                continue;
            }

            SavedPick pick;
            IADOccurrencePtr occurrence = proxy->GetOccurrence();

            PocLog(L"  [%ld] display name: %s", i, static_cast<const wchar_t*>(proxy->GetDisplayName()));

            if (occurrence != nullptr)
            {
                pick.occurrence = PocTakeKey(occurrence->GetKey());
                pick.occurrenceName = static_cast<const wchar_t*>(occurrence->GetName());
                PocLog(L"    occurrence %s nestLevel=%ld keyVt=%d keyBytes=%zu", pick.occurrenceName.c_str(),
                       occurrence->GetNestLevel(), static_cast<int>(pick.occurrence.vt),
                       pick.occurrence.hex.size() / 2);

                PocGuard(L"occurrence units", [&]
                {
                    PocLog(L"    part model units=%s", DesignUnits(occurrence->GetDesignSession()));
                });
                PocGuard(L"occurrence transform", [&]
                {
                    PocLog(L"    world transform: %s",
                           static_cast<const wchar_t*>(occurrence->GetWorldTransform()->GetToString()));
                });
            }

            IDispatchPtr target = proxy->GetTarget();
            if (target == nullptr)
            {
                PocLog(L"    no target: the whole occurrence was selected");
                continue;
            }

            PocGuard(L"read target", [&]
            {
                LogPickedTarget(occurrence, target, pick.target, pick.kind, pick.timeStamp);
            });

            if (!pick.occurrence.IsEmpty() && !pick.target.IsEmpty())
                AppendSavedPick(pick);
        }

        SceneChanged();
    });
}

/////////////////////////////////////////////////////////////////////////////
// Test: do saved keys still bind?

void CCamTabCommand::BindSavedKeys()
{
    const std::vector<SavedPick> picks = ReadSavedPicks();
    PocLog(L"[keys] binding %zu saved picks from %s", picks.size(), KeysPath().c_str());

    PocGuard(L"bind keys", [&]
    {
        IADSessionPtr session = GetSession();

        int index = 0;
        for (const SavedPick& pick : picks)
        {
            PocLog(L"  pick %d: %s on %s, saved timeStamp=%ld", index++, pick.kind.c_str(),
                   pick.occurrenceName.c_str(), pick.timeStamp);

            IADOccurrencePtr occurrence;
            PocGuard(L"bind occurrence key", [&]
            {
                occurrence = PocQI<IADOccurrencePtr>(BindKey(session, pick.occurrence, ADObjectType_AD_OCCURRENCE));
            });
            if (occurrence == nullptr)
            {
                PocLog(L"    occurrence key: NOT BOUND");
                continue;
            }
            PocLog(L"    occurrence key: bound to %s", static_cast<const wchar_t*>(occurrence->GetName()));

            // Try the target key against both the part and the assembly, since
            // the documentation doesn't say which session owns it.
            IDispatchPtr targetInPart;
            PocGuard(L"bind target in part", [&]
            {
                IADSessionPtr partSession = PocQI<IADSessionPtr>(occurrence->GetDesignSession());
                targetInPart = BindKey(partSession, pick.target, ADObjectType_AD_TOPOLOGY);
            });
            PocLog(L"    target key in part session: %s", targetInPart != nullptr ? L"bound" : L"NOT BOUND");

            IDispatchPtr targetInAssembly;
            PocGuard(L"bind target in assembly", [&]
            {
                targetInAssembly = BindKey(session, pick.target, ADObjectType_AD_TOPOLOGY);
            });
            PocLog(L"    target key in assembly session: %s", targetInAssembly != nullptr ? L"bound" : L"NOT BOUND");

            IDispatchPtr target = (targetInPart != nullptr) ? targetInPart : targetInAssembly;
            if (target == nullptr)
                continue;

            PocGuard(L"read bound target", [&]
            {
                IADPointPtr lower, upper;
                if (IADFacePtr face = PocQI<IADFacePtr>(target))
                {
                    face->GetExtents(&lower, &upper);
                    PocLog(L"    is a face, timeStamp now=%ld (%s)", face->GetTimeStamp(),
                           face->GetTimeStamp() == pick.timeStamp ? L"unchanged" : L"CHANGED");
                }
                else if (IADEdgePtr edge = PocQI<IADEdgePtr>(target))
                {
                    edge->GetExtents(&lower, &upper);
                    PocLog(L"    is an edge, timeStamp now=%ld (%s)", edge->GetTimeStamp(),
                           edge->GetTimeStamp() == pick.timeStamp ? L"unchanged" : L"CHANGED");
                }
                else if (PocQI<IADVertexPtr>(target) != nullptr)
                {
                    PocLog(L"    is a vertex");
                }
                LogExtents(lower, upper);
            });
        }
    });
}

void CCamTabCommand::ClearSavedKeys()
{
    const BOOL deleted = DeleteFileW(KeysPath().c_str());
    PocLog(L"[keys] saved picks cleared (%s)", deleted ? L"deleted" : L"no file");
    m_scene.clickMarkers.clear();
    m_scene.entityMarkers.clear();
    SceneChanged();
}

/////////////////////////////////////////////////////////////////////////////
// Test: load a part with no editor window, read it, and close it

void CCamTabCommand::TestStockLoad(CWnd* pParent)
{
    CFileDialog dialog(TRUE, L"AD_PRT", nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                       L"Alibre parts (*.AD_PRT)|*.AD_PRT||", pParent);
    if (dialog.DoModal() != IDOK)
        return;

    const CString path = dialog.GetPathName();
    PocLog(L"[stock] loading %s", static_cast<LPCWSTR>(path));

    PocGuard(L"stock load", [&]
    {
        IADSessionsPtr sessions = theApp.m_pRoot->GetSessions();
        PocLog(L"  sessions before: %ld", sessions->GetCount());

        const ULONGLONG start = GetTickCount64();
        IADSessionPtr stock = theApp.m_pRoot->OpenFileEx(_bstr_t(static_cast<LPCWSTR>(path)), VARIANT_FALSE);
        PocLog(L"  OpenFileEx took %llu ms, sessions now: %ld", GetTickCount64() - start, sessions->GetCount());

        if (stock == nullptr)
        {
            PocLog(L"  OpenFileEx returned nothing");
            return;
        }

        PocLog(L"  name=%s guiVisible=%d", static_cast<const wchar_t*>(stock->GetName()),
               stock->GetIsGUIVisible() != VARIANT_FALSE ? 1 : 0);

        PocGuard(L"stock units", [&]
        {
            PocLog(L"  model units=%s", DesignUnits(PocQI<IADDesignSessionPtr>(stock)));
        });

        if (IADPartSessionPtr part = PocQI<IADPartSessionPtr>(stock))
        {
            PocLog(L"  bodies=%ld", part->GetBodies()->GetCount());

            SAFEARRAY* pFacets = part->GetFacetData();
            if (pFacets != nullptr)
            {
                VARTYPE vt = VT_EMPTY;
                SafeArrayGetVartype(pFacets, &vt);
                LONG lower = 0, upper = -1;
                SafeArrayGetLBound(pFacets, 1, &lower);
                SafeArrayGetUBound(pFacets, 1, &upper);
                const LONG count = upper - lower + 1;
                PocLog(L"  facet data: vt=%d values=%ld triangles=%ld", static_cast<int>(vt), count, count / 9);

                double* pData = nullptr;
                if (vt == VT_R8 && count >= 3 &&
                    SUCCEEDED(SafeArrayAccessData(pFacets, reinterpret_cast<void**>(&pData))))
                {
                    double lo[3] = { pData[0], pData[1], pData[2] };
                    double hi[3] = { pData[0], pData[1], pData[2] };
                    for (LONG i = 0; i + 2 < count; i += 3)
                    {
                        for (int axis = 0; axis < 3; ++axis)
                        {
                            const double value = pData[i + axis];
                            if (value < lo[axis]) lo[axis] = value;
                            if (value > hi[axis]) hi[axis] = value;
                        }
                    }
                    SafeArrayUnaccessData(pFacets);

                    PocLog(L"  bounding box (%.5f, %.5f, %.5f) to (%.5f, %.5f, %.5f)",
                           lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);

                    for (int axis = 0; axis < 3; ++axis)
                    {
                        m_scene.stockMin[axis] = static_cast<float>(lo[axis]);
                        m_scene.stockMax[axis] = static_cast<float>(hi[axis]);
                    }
                    m_scene.hasStock = true;
                }
                SafeArrayDestroy(pFacets);
            }
        }
        else
        {
            PocLog(L"  the session is not a part");
        }

        stock->Close(VARIANT_FALSE);
        PocLog(L"  closed, sessions now: %ld", sessions->GetCount());

        SceneChanged();
    });
}

void CCamTabCommand::ToggleBigPolyline()
{
    m_scene.bigPolylinePoints = (m_scene.bigPolylinePoints == 0) ? kBigPolylinePoints : 0;
    PocLog(L"[draw] large polyline: %d points", m_scene.bigPolylinePoints);
    SceneChanged();
}

void CCamTabCommand::SetDrawMode(bool foreground, bool redrawEveryRender)
{
    m_scene.foreground = foreground;
    m_scene.redrawEveryRender = redrawEveryRender;
    PocLog(L"[draw] mode: foreground=%d redrawEveryRender=%d", foreground ? 1 : 0, redrawEveryRender ? 1 : 0);
    SceneChanged();
}

void CCamTabCommand::CyclePickFilter()
{
    // Alibre documents the vertex options as invalid for the Solid filter in an
    // assembly. They're included to test that against the running version.
    struct FilterOption
    {
        ADSelectionFilterOption option;
        const wchar_t* name;
    };
    static const FilterOption options[] =
    {
        { ADSelectionFilterOption_AD_FACES,       L"faces" },
        { ADSelectionFilterOption_AD_EDGES,       L"edges" },
        { ADSelectionFilterOption_AD_PARTS,       L"parts" },
        { ADSelectionFilterOption_AD_VERTICES,    L"vertices" },
        { ADSelectionFilterOption_AD_FACES_EDGES_VERTICES, L"faces, edges and vertices" },
        { ADSelectionFilterOption_AD_FACES_EDGES, L"faces and edges" }
    };

    const FilterOption& next = options[m_pickFilterIndex];
    m_pickFilterIndex = (m_pickFilterIndex + 1) % static_cast<int>(_countof(options));

    PocGuard(L"pick filter", [&]
    {
        IADDesignSessionPtr design = PocQI<IADDesignSessionPtr>(GetSession());
        IADDesignSelectionFilterPtr filter = design->GetSelectionFilter();
        const int previous = static_cast<int>(filter->GetSolid());
        filter->PutSolid(next.option);
        PocLog(L"[pick] filter was %d, now %s (reads back %d)", previous, next.name,
               static_cast<int>(filter->GetSolid()));
    });
}

/////////////////////////////////////////////////////////////////////////////
// CCamTabWnd

BEGIN_MESSAGE_MAP(CCamTabWnd, CWnd)
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_SIZE()
    ON_WM_SHOWWINDOW()
    ON_WM_TIMER()
    ON_BN_CLICKED(IDC_CAM_SHOW, &CCamTabWnd::OnRefreshToolpaths)
    ON_BN_CLICKED(IDC_CAM_HIDE, &CCamTabWnd::OnHideToolpaths)
    ON_BN_CLICKED(IDC_CAM_BIG, &CCamTabWnd::OnBigPolyline)
    ON_BN_CLICKED(IDC_CAM_FILTER, &CCamTabWnd::OnPickFilter)
    ON_BN_CLICKED(IDC_CAM_FOREGROUND, &CCamTabWnd::OnDrawModeChanged)
    ON_BN_CLICKED(IDC_CAM_REDRAW, &CCamTabWnd::OnDrawModeChanged)
    ON_BN_CLICKED(IDC_CAM_BIND, &CCamTabWnd::OnBindKeys)
    ON_BN_CLICKED(IDC_CAM_CLEAR, &CCamTabWnd::OnClearKeys)
    ON_BN_CLICKED(IDC_CAM_STOCK, &CCamTabWnd::OnTestStock)
    ON_BN_CLICKED(IDC_CAM_OPENLOG, &CCamTabWnd::OnOpenLog)
    ON_NOTIFY(TVN_SELCHANGED, IDC_CAM_TREE, &CCamTabWnd::OnTreeSelChanged)
END_MESSAGE_MAP()

CCamTabWnd::CCamTabWnd(CCamTabCommand* pTab)
    : m_pTab(pTab)
    , m_visible(false)
{
}

bool CCamTabWnd::CreateInTab(HWND hParent)
{
    LogParentChain(hParent);

    CRect client;
    ::GetClientRect(hParent, &client);

    const CString windowClass = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW),
                                                    reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_BTNFACE + 1)));

    return CreateEx(0, windowClass, L"CAM", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, client,
                    CWnd::FromHandle(hParent), 1) != FALSE;
}

void CCamTabWnd::AddButton(CButton& button, const wchar_t* text, UINT id, DWORD style)
{
    button.Create(text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style, CRect(0, 0, 10, 10), this, id);
    button.SetFont(CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT))));
}

int CCamTabWnd::OnCreate(LPCREATESTRUCT pCreate)
{
    if (CWnd::OnCreate(pCreate) == -1)
        return -1;

    CFont* pFont = CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));

    // Tests that a tree view works inside the tab. These are the fixed nodes
    // from the specification.
    m_tree.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES |
                      TVS_LINESATROOT | TVS_SHOWSELALWAYS,
                  CRect(0, 0, 10, 10), this, IDC_CAM_TREE);
    m_tree.SetFont(pFont);
    HTREEITEM hSetup = m_tree.InsertItem(L"Setup");
    m_tree.InsertItem(L"Machine", hSetup);
    m_tree.InsertItem(L"Tools", hSetup);
    m_tree.InsertItem(L"Stock", hSetup);
    HTREEITEM hOperations = m_tree.InsertItem(L"Operations");
    m_tree.InsertItem(L"Set Origin: Top", hOperations);
    m_tree.Expand(hSetup, TVE_EXPAND);
    m_tree.Expand(hOperations, TVE_EXPAND);

    AddButton(m_auto, L"Auto show on tab focus", IDC_CAM_AUTO, BS_AUTOCHECKBOX);
    m_auto.SetCheck(BST_CHECKED);
    AddButton(m_foreground, L"Draw over the model (foreground)", IDC_CAM_FOREGROUND, BS_AUTOCHECKBOX);
    AddButton(m_redraw, L"Redraw on every render", IDC_CAM_REDRAW, BS_AUTOCHECKBOX);
    m_foreground.SetCheck(BST_CHECKED);
    m_redraw.SetCheck(BST_CHECKED);
    AddButton(m_show, L"Refresh toolpaths", IDC_CAM_SHOW, BS_PUSHBUTTON);
    AddButton(m_hide, L"Hide toolpaths", IDC_CAM_HIDE, BS_PUSHBUTTON);
    AddButton(m_big, L"Large polyline on/off", IDC_CAM_BIG, BS_PUSHBUTTON);
    AddButton(m_filter, L"Next pick filter", IDC_CAM_FILTER, BS_PUSHBUTTON);
    AddButton(m_bind, L"Bind saved keys", IDC_CAM_BIND, BS_PUSHBUTTON);
    AddButton(m_clear, L"Clear saved keys", IDC_CAM_CLEAR, BS_PUSHBUTTON);
    AddButton(m_stock, L"Test stock load...", IDC_CAM_STOCK, BS_PUSHBUTTON);
    AddButton(m_openLog, L"Open log file", IDC_CAM_OPENLOG, BS_PUSHBUTTON);

    m_log.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOSEL,
                 CRect(0, 0, 10, 10), this, IDC_CAM_LOG);
    m_log.SetFont(pFont);
    m_log.SetHorizontalExtent(2000);
    PocLogAddList(m_log.GetSafeHwnd());

    CRect client;
    GetClientRect(&client);
    LayoutControls(client.Width(), client.Height());

    // Alibre has no documented tab-focus event, so poll the window state.
    SetTimer(kPollTimer, kPollMilliseconds, nullptr);
    return 0;
}

void CCamTabWnd::OnDestroy()
{
    KillTimer(kPollTimer);
    PocLogRemoveList(m_log.GetSafeHwnd());
    CWnd::OnDestroy();
}

void CCamTabWnd::PostNcDestroy()
{
    if (m_pTab != nullptr)
        m_pTab->OnWindowGone();
    delete this;
}

void CCamTabWnd::LayoutControls(int cx, int cy)
{
    if (m_tree.GetSafeHwnd() == nullptr)
        return;

    const int margin = 4;
    const int rowHeight = 23;
    const int width = (cx > margin * 2) ? cx - margin * 2 : 1;
    int y = margin;

    const auto place = [&](CWnd& control, int height)
    {
        control.MoveWindow(margin, y, width, height);
        y += height + margin;
    };

    place(m_tree, 140);
    place(m_auto, rowHeight);
    place(m_foreground, rowHeight);
    place(m_redraw, rowHeight);
    place(m_show, rowHeight);
    place(m_hide, rowHeight);
    place(m_big, rowHeight);
    place(m_filter, rowHeight);
    place(m_bind, rowHeight);
    place(m_clear, rowHeight);
    place(m_stock, rowHeight);
    place(m_openLog, rowHeight);

    const int remaining = cy - y - margin;
    m_log.MoveWindow(margin, y, width, (remaining > 40) ? remaining : 40);
}

void CCamTabWnd::OnSize(UINT type, int cx, int cy)
{
    CWnd::OnSize(type, cx, cy);
    LayoutControls(cx, cy);
}

void CCamTabWnd::OnShowWindow(BOOL show, UINT status)
{
    CWnd::OnShowWindow(show, status);
    PocLog(L"[focus] WM_SHOWWINDOW show=%d status=%u", show ? 1 : 0, status);
}

void CCamTabWnd::LogParentChain(HWND hParent)
{
    int depth = 0;
    for (HWND hWnd = hParent; hWnd != nullptr && depth < 12; hWnd = ::GetParent(hWnd), ++depth)
    {
        wchar_t className[128] = {};
        ::GetClassNameW(hWnd, className, _countof(className));
        CRect rect;
        ::GetWindowRect(hWnd, &rect);
        PocLog(L"[focus] parent %d: class=%s visible=%d size=%dx%d", depth, className,
               ::IsWindowVisible(hWnd) ? 1 : 0, rect.Width(), rect.Height());
    }
}

void CCamTabWnd::OnTimer(UINT_PTR id)
{
    if (id == kPollTimer)
        PollVisibility();
    else
        CWnd::OnTimer(id);
}

void CCamTabWnd::PollVisibility()
{
    // The window Alibre created for the tab may not resize this one.
    CWnd* pParent = GetParent();
    if (pParent != nullptr)
    {
        CRect parentClient, own;
        pParent->GetClientRect(&parentClient);
        GetClientRect(&own);
        if (parentClient.Width() != own.Width() || parentClient.Height() != own.Height())
            MoveWindow(0, 0, parentClient.Width(), parentClient.Height());
    }

    CRect rect;
    GetWindowRect(&rect);
    if (rect != m_lastRect)
    {
        PocLog(L"[focus] window rect now (%d, %d) %dx%d", rect.left, rect.top, rect.Width(), rect.Height());
        m_lastRect = rect;
    }

    const bool visible = (::IsWindowVisible(m_hWnd) != FALSE);
    if (visible == m_visible)
        return;

    m_visible = visible;
    PocLog(L"[focus] tab %s (IsWindowVisible=%d)", visible ? L"GAINED focus" : L"LOST focus", visible ? 1 : 0);

    if (m_pTab == nullptr || m_auto.GetCheck() != BST_CHECKED)
        return;

    if (visible)
        m_pTab->StartView(L"tab gained focus");
    else
        m_pTab->StopView(L"tab lost focus");
}

void CCamTabWnd::OnRefreshToolpaths()
{
    if (m_pTab != nullptr)
        m_pTab->RefreshView();
}

void CCamTabWnd::OnHideToolpaths()
{
    if (m_pTab != nullptr)
        m_pTab->StopView(L"button");
}

void CCamTabWnd::OnBigPolyline()
{
    if (m_pTab != nullptr)
        m_pTab->ToggleBigPolyline();
}

void CCamTabWnd::OnDrawModeChanged()
{
    if (m_pTab != nullptr)
        m_pTab->SetDrawMode(m_foreground.GetCheck() == BST_CHECKED, m_redraw.GetCheck() == BST_CHECKED);
}

void CCamTabWnd::OnPickFilter()
{
    if (m_pTab != nullptr)
        m_pTab->CyclePickFilter();
}

void CCamTabWnd::OnBindKeys()
{
    if (m_pTab != nullptr)
        m_pTab->BindSavedKeys();
}

void CCamTabWnd::OnClearKeys()
{
    if (m_pTab != nullptr)
        m_pTab->ClearSavedKeys();
}

void CCamTabWnd::OnTestStock()
{
    if (m_pTab != nullptr)
        m_pTab->TestStockLoad(this);
}

void CCamTabWnd::OnOpenLog()
{
    ::ShellExecuteW(m_hWnd, L"open", PocLogPath().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void CCamTabWnd::OnTreeSelChanged(NMHDR* /*pNotify*/, LRESULT* pResult)
{
    HTREEITEM hItem = m_tree.GetSelectedItem();
    if (hItem != nullptr)
        PocLog(L"[tree] selected: %s", static_cast<LPCWSTR>(m_tree.GetItemText(hItem)));
    *pResult = 0;
}
