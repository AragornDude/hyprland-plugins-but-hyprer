#pragma once

#define WLR_USE_UNSTABLE

#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <hyprland/src/devices/ITouch.hpp>
#include <hyprland/src/desktop/WindowRule.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include "globals.hpp"

#define private public
#include <hyprland/src/managers/input/InputManager.hpp>
#undef private

class CHyprBar : public IHyprWindowDecoration {
  public:
    CHyprBar(PHLWINDOW);
    virtual ~CHyprBar();

    virtual SDecorationPositioningInfo getPositioningInfo();

    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply);

    virtual void                       draw(PHLMONITOR, float const& a);

    virtual eDecorationType            getDecorationType();

    virtual void                       updateWindow(PHLWINDOW);

    virtual void                       damageEntire();

    virtual eDecorationLayer           getDecorationLayer();

    virtual uint64_t                   getDecorationFlags();

    bool                               m_bButtonsDirty = true;

    virtual std::string                getDisplayName();

    PHLWINDOW                          getOwner();

    void                               updateRules();
    void                               applyRule(const SP<CWindowRule>&);

    WP<CHyprBar>                       m_self;

  private:
    SBoxExtents               m_seExtents;

    PHLWINDOWREF              m_pWindow;

    CBox                      m_bAssignedBox;

    SP<CTexture>              m_pTextTex;
    SP<CTexture>              m_pButtonsTex;

    bool                      m_bWindowSizeChanged = false;
    bool                      m_hidden             = false;
    bool                      m_bTitleColorChanged = false;
    bool                      m_bButtonHovered     = false;
    bool                      m_bLastEnabledState  = false;
    bool                      m_bWindowHasFocus    = false;

    // Bar Window Rules
    std::optional<int> m_bForcedBarHeight;
    std::optional<int> m_bForcedBarPadding;
    std::optional<CHyprColor> m_bForcedBarColor;
    std::optional<int> m_bForcedBarBlur;
    std::optional<int> m_bForcedBarPartOfWindow;
    std::optional<int> m_bForcedBarPrecedenceOverBorder;
    std::optional<std::string> m_bForcedOnDoubleClick;
    // Title Window Rules
    std::optional<int>m_bForcedBarTitleEnabled;
    std::optional<std::string> m_bForcedBarTextFont;
    std::optional<int> m_bForcedBarTextSize;
    std::optional<std::string> m_bForcedBarTextAlign;
    std::optional<CHyprColor> m_bForcedTitleColor;
    std::optional<std::string> m_bForcedBarCustomTitle;
    // Buttons Window Rules
    std::optional<int> m_bForcedIconOnHover;
    std::optional<std::string> m_bForcedBarButtonsAlignment;
    std::optional<int> m_bForcedBarButtonPadding;
    std::optional<CHyprColor> m_bForcedInactiveButtonColor;
    struct WindowRuleButton {
      CHyprColor bgcol;
      int size;
      std::string icon;
      std::string cmd;
      std::optional<CHyprColor> fgcol;
      SP<CTexture> iconTex = makeShared<CTexture>();
      bool userfg = false;
    };
    std::vector<WindowRuleButton> m_windowRuleButtons;

    Time::steady_tp           m_lastMouseDown = Time::steadyNow();

    PHLANIMVAR<CHyprColor>    m_cRealBarColor;

    Vector2D                  cursorRelativeToBar();

    void                      renderPass(PHLMONITOR, float const& a);
    void                      renderBarTitle(const Vector2D& bufferSize, const float scale);
    void                      renderText(SP<CTexture> out, const std::string& text, const CHyprColor& color, const Vector2D& bufferSize, const float scale, const int fontSize);
    void                      renderBarButtons(const Vector2D& bufferSize, const float scale);
    void                      renderBarButtonsText(CBox* barBox, const float scale, const float a);
    void                      damageOnButtonHover();

    bool                      inputIsValid();
    void                      onMouseButton(SCallbackInfo& info, IPointer::SButtonEvent e);
    void                      onTouchDown(SCallbackInfo& info, ITouch::SDownEvent e);
    void                      onMouseMove(Vector2D coords);
    void                      onTouchMove(SCallbackInfo& info, ITouch::SMotionEvent e);

    void                      handleDownEvent(SCallbackInfo& info, std::optional<ITouch::SDownEvent> touchEvent);
    void                      handleUpEvent(SCallbackInfo& info);
    void                      handleMovement();
    bool doButtonPress(Hyprlang::INT* const* PBARPADDING, Hyprlang::INT* const* PBARBUTTONPADDING, Hyprlang::INT* const* PHEIGHT, Vector2D COORDS, bool BUTTONSRIGHT);

    CBox assignedBoxGlobal();

    SP<HOOK_CALLBACK_FN> m_pMouseButtonCallback;
    SP<HOOK_CALLBACK_FN> m_pTouchDownCallback;
    SP<HOOK_CALLBACK_FN> m_pTouchUpCallback;

    SP<HOOK_CALLBACK_FN> m_pTouchMoveCallback;
    SP<HOOK_CALLBACK_FN> m_pMouseMoveCallback;

    std::string          m_szLastTitle;

    bool                 m_bDraggingThis  = false;
    bool                 m_bTouchEv       = false;
    bool                 m_bDragPending   = false;
    bool                 m_bCancelledDown = false;

    // store hover state for buttons as a bitfield
    unsigned int m_iButtonHoverState = 0;

    // for dynamic updates
    int    m_iLastHeight = 0;
    int    m_iLastTextSize = -1;

    size_t getVisibleButtonCount(Hyprlang::INT* const* PBARBUTTONPADDING, Hyprlang::INT* const* PBARPADDING, const Vector2D& bufferSize, const float scale);

    friend class CBarPassElement;
};
