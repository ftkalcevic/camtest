// CamViewCommand.h : the regular command that shows toolpaths and receives picks.
//
// Alibre only draws add-on graphics while a regular command is active, and any
// other command ends it. The CAM tab starts this command when it gains focus.

#pragma once

#include "PocCommandBase.h"

class CCamTabCommand;

class CCamViewCommand : public CPocCommandBase
{
public:
    explicit CCamViewCommand(CCamTabCommand* pTab);
    virtual ~CCamViewCommand();

    // Asks Alibre to redraw, which calls On3DRender.
    void Refresh();

    // IAlibreAddOnCommand
    STDMETHOD(OnClick)(long screenX, long screenY, enum ADDONMouseButtons buttons,
                       VARIANT_BOOL* pHandled) override;
    STDMETHOD(OnDoubleClick)(long screenX, long screenY, VARIANT_BOOL* pHandled) override;
    STDMETHOD(OnMouseDown)(long screenX, long screenY, enum ADDONMouseButtons buttons,
                           VARIANT_BOOL* pHandled) override;
    STDMETHOD(OnKeyDown)(long keycode, VARIANT_BOOL* pHandled) override;
    STDMETHOD(OnSelectionChange)() override;
    STDMETHOD(OnTerminate)() override;
    STDMETHOD(OnComplete)() override;
    STDMETHOD(On3DRender)() override;

private:
    void DrawScene(IADAddOnCanvasDisplay* pCanvas);

    CCamTabCommand* m_pTab;     // holds a reference until OnTerminate
    int m_drawnRevision;
    bool m_reportedRender;

    // Counts calls so the log shows whether pan, zoom and hover cause renders.
    long m_renderCalls;
    long m_redraws;
    ULONGLONG m_lastRenderLog;
};
