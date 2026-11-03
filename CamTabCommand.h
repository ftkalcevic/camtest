// CamTabCommand.h : the two-way toggle command that owns the CAM tab.
//
// There is one of these per assembly workspace. It hosts the tab window, starts
// and stops the view command, and runs the API tests started from the tab.

#pragma once

#include "PocCommandBase.h"
#include "PocHelpers.h"

class CAddOnInterface;
class CCamViewCommand;
class CCamTabWnd;

struct CamMarker
{
    float x, y, z;
};

// What the view command draws. A new revision makes it redraw.
struct CamScene
{
    int revision = 0;
    int bigPolylinePoints = 0;
    bool foreground = true;             // draw over the model, ignoring depth
    bool redrawEveryRender = true;      // Alibre drops the graphics on each render
    bool hasStock = false;
    float stockMin[3] = {};
    float stockMax[3] = {};
    std::vector<CamMarker> clickMarkers;
    std::vector<CamMarker> entityMarkers;
};

class CCamTabCommand : public CPocCommandBase
{
public:
    CCamTabCommand(CAddOnInterface* pAddOn, const std::wstring& sessionId);
    virtual ~CCamTabCommand();

    const CamScene& GetScene() const { return m_scene; }

    // Starts the view command through InvokeNewCommand on this command's site.
    void StartView(const wchar_t* reason);
    void StopView(const wchar_t* reason);
    void RefreshView();

    // Ribbon fallback: creates a view command for InvokeCommand to return.
    HRESULT CreateViewForRibbon(IAlibreAddOnCommand** ppCommand);

    void OnViewTerminated(CCamViewCommand* pView);
    void OnWindowGone();
    void CloseFromAddOn();

    // API tests
    void HandleSelection();
    void BindSavedKeys();
    void ClearSavedKeys();
    void TestStockLoad(CWnd* pParent);
    void ToggleBigPolyline();
    void SetDrawMode(bool foreground, bool redrawEveryRender);
    void CyclePickFilter();

    // IAlibreAddOnCommand
    STDMETHOD(IsTwoWayToggle)(VARIANT_BOOL* pValue) override;
    STDMETHOD(AddTab)(VARIANT_BOOL* pValue) override;
    STDMETHOD(get_TabName)(BSTR* pName) override;
    STDMETHOD(OnShowUI)(__int64 hWnd) override;
    STDMETHOD(OnTerminate)() override;

private:
    IADSessionPtr GetSession() const;
    void SceneChanged();
    void LogPickedTarget(IADOccurrence* pOccurrence, IDispatch* pTarget, PocKey& targetKey,
                         std::wstring& kind, long& timeStamp);

    CAddOnInterface* m_pAddOn;
    std::wstring m_sessionId;
    CCamTabWnd* m_pWnd;
    CCamViewCommand* m_pView;   // holds a reference while the view is active
    int m_pickFilterIndex;
    CamScene m_scene;
};

/////////////////////////////////////////////////////////////////////////////
// The window Alibre hosts in the Design Explorer tab.

class CCamTabWnd : public CWnd
{
public:
    explicit CCamTabWnd(CCamTabCommand* pTab);

    bool CreateInTab(HWND hParent);
    void DetachFromCommand() { m_pTab = nullptr; }

protected:
    afx_msg int OnCreate(LPCREATESTRUCT pCreate);
    afx_msg void OnDestroy();
    afx_msg void OnSize(UINT type, int cx, int cy);
    afx_msg void OnShowWindow(BOOL show, UINT status);
    afx_msg void OnTimer(UINT_PTR id);
    afx_msg void OnRefreshToolpaths();
    afx_msg void OnHideToolpaths();
    afx_msg void OnBigPolyline();
    afx_msg void OnDrawModeChanged();
    afx_msg void OnPickFilter();
    afx_msg void OnBindKeys();
    afx_msg void OnClearKeys();
    afx_msg void OnTestStock();
    afx_msg void OnOpenLog();
    afx_msg void OnTreeSelChanged(NMHDR* pNotify, LRESULT* pResult);

    void PostNcDestroy() override;

    DECLARE_MESSAGE_MAP()

private:
    void LayoutControls(int cx, int cy);
    void LogParentChain(HWND hParent);
    void PollVisibility();
    void AddButton(CButton& button, const wchar_t* text, UINT id, DWORD style);

    CCamTabCommand* m_pTab;
    CTreeCtrl m_tree;
    CButton m_auto;
    CButton m_foreground;
    CButton m_redraw;
    CButton m_show;
    CButton m_hide;
    CButton m_big;
    CButton m_filter;
    CButton m_bind;
    CButton m_clear;
    CButton m_stock;
    CButton m_openLog;
    CListBox m_log;

    bool m_visible;
    CRect m_lastRect;
};
