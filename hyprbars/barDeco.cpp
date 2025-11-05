#include "barDeco.hpp"
#include <hyprland/src/plugins/PluginAPI.hpp>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/protocols/LayerShell.hpp>
#include <cairo/cairo.h>
#include <pango/pango.h>
#include <pango/pango-layout.h>
#include <pango/pango-font.h>
#include <pango/pangocairo.h>
#include <glib-object.h>
#include "hyprbars_utils.hpp"
#include "hyprbars_logger.hpp"
#include <algorithm>
#include <limits>
//
// Define missing layer shell constants if not present
#ifndef ZWLR_LAYER_SHELL_V1_LAYER_TOP
#define ZWLR_LAYER_SHELL_V1_LAYER_TOP 2
#endif
#ifndef ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY
#define ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY 3
#endif

#include "globals.hpp"
#include "BarPassElement.hpp"

#include <optional>
#include <cmath>

// Use configStringToIntOpt from hyprbars_utils.hpp

// Use Hyprland's configStringToInt (returns std::expected<int64_t,std::string>).
// Provide a safe fallback for DAMAGE_NONE if not defined by the headers.
#ifndef DAMAGE_NONE
#ifdef AVARDAMAGE_NONE
#define DAMAGE_NONE AVARDAMAGE_NONE
#else
#define DAMAGE_NONE 0
#endif
#endif

// Substitute variables in custom title templates. Templates can include
// {Name} entries coming from config `title-var` keywords (populated into
// `g_titleVars`), as well as many runtime window properties. This mirrors
// the behavior in the older fork so users can craft rich custom titles.
std::string substituteTitleVars(const std::string& tpl, PHLWINDOW PWINDOW) {
    std::string result = tpl;
    // Replace variables
    auto replaceAll = [](std::string& str, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }
    };
    auto vecToStr = [](const Vector2D& v) {
        return std::to_string(v.x) + "," + std::to_string(v.y);
    };
    auto colorToStr = [](const CHyprColor& c) {
        return std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b) + "," + std::to_string(c.a);
    };
    
    for (const auto& [name, value] : g_titleVars) {
        replaceAll(result, "{" + name + "}", value);
    }

    // Date and Time
    std::time_t t = std::time(nullptr);
    std::tm tm;
    localtime_r(&t, &tm);
    char dateBuf[32], timeBuf[32];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &tm);
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm);


    // Hyprctl Window Variables Working (those returned by 'hyprctl clients')
    // Strings
    replaceAll(result, "{Title}", PWINDOW->m_title);
    replaceAll(result, "{Class}", PWINDOW->m_class);
    replaceAll(result, "{InitialTitle}", PWINDOW->m_initialTitle);
    replaceAll(result, "{InitialClass}", PWINDOW->m_initialClass);
    replaceAll(result, "{initialWorkspaceToken}", PWINDOW->m_initialWorkspaceToken);
    // Bools Ints and Floats
    replaceAll(result, "{isPseudotiled}", PWINDOW->m_isPseudotiled ? "true" : "false");
    replaceAll(result, "{firstMap}", PWINDOW->m_firstMap ? "true" : "false");
    replaceAll(result, "{isFloating}", PWINDOW->m_isFloating ? "true" : "false");
    replaceAll(result, "{draggingTiled}", PWINDOW->m_draggingTiled ? "true" : "false");
    replaceAll(result, "{isMapped}", PWINDOW->m_isMapped ? "true" : "false");
    replaceAll(result, "{requestsFloat}", PWINDOW->m_requestsFloat ? "true" : "false");
    replaceAll(result, "{createdOverFullscreen}", PWINDOW->m_createdOverFullscreen ? "true" : "false");
    replaceAll(result, "{isX11}", PWINDOW->m_isX11 ? "true" : "false");
    replaceAll(result, "{X11DoesntWantBorders}", PWINDOW->m_X11DoesntWantBorders ? "true" : "false");
    replaceAll(result, "{X11ShouldntFocus}", PWINDOW->m_X11ShouldntFocus ? "true" : "false");
    replaceAll(result, "{noInitialFocus}", PWINDOW->m_noInitialFocus ? "true" : "false");
    replaceAll(result, "{wantsInitialFullscreen}", PWINDOW->m_wantsInitialFullscreen ? "true" : "false");
    replaceAll(result, "{fadingOut}", PWINDOW->m_fadingOut ? "true" : "false");
    replaceAll(result, "{readyToDelete}", PWINDOW->m_readyToDelete ? "true" : "false");
    replaceAll(result, "{animatingIn}", PWINDOW->m_animatingIn ? "true" : "false");
    replaceAll(result, "{pinned}", PWINDOW->m_pinned ? "true" : "false");
    replaceAll(result, "{pinFullscreened}", PWINDOW->m_pinFullscreened ? "true" : "false");
    replaceAll(result, "{isUrgent}", PWINDOW->m_isUrgent ? "true" : "false");
    replaceAll(result, "{monitorMovedFrom}", std::to_string(PWINDOW->m_monitorMovedFrom));
    replaceAll(result, "{currentlySwallowed}", PWINDOW->m_currentlySwallowed ? "true" : "false");
    replaceAll(result, "{groupSwallowed}", PWINDOW->m_groupSwallowed ? "true" : "false");
    replaceAll(result, "{stayFocused}", PWINDOW->m_stayFocused ? "true" : "false");
    replaceAll(result, "{tearingHint}", PWINDOW->m_tearingHint ? "true" : "false");
    replaceAll(result, "{X11SurfaceScaledBy}", std::to_string(PWINDOW->m_X11SurfaceScaledBy));
    // Vector2D
    replaceAll(result, "{floatingOffset}", vecToStr(PWINDOW->m_floatingOffset));
    replaceAll(result, "{lastFloatingPosition}", vecToStr(PWINDOW->m_lastFloatingPosition));
    replaceAll(result, "{lastFloatingSize}", vecToStr(PWINDOW->m_lastFloatingSize));
    replaceAll(result, "{originalClosedPos}", vecToStr(PWINDOW->m_originalClosedPos));
    replaceAll(result, "{originalClosedSize}", vecToStr(PWINDOW->m_originalClosedSize));
    replaceAll(result, "{pendingReportedSize}", vecToStr(PWINDOW->m_pendingReportedSize));
    replaceAll(result, "{position}", vecToStr(PWINDOW->m_position));
    replaceAll(result, "{pseudoSize}", vecToStr(PWINDOW->m_pseudoSize));
    replaceAll(result, "{relativeCursorCoordsOnLastWarp}", vecToStr(PWINDOW->m_relativeCursorCoordsOnLastWarp));
    replaceAll(result, "{reportedPosition}", vecToStr(PWINDOW->m_reportedPosition));
    replaceAll(result, "{reportedSize}", vecToStr(PWINDOW->m_reportedSize));
    replaceAll(result, "{size}", vecToStr(PWINDOW->m_size));
    // PHL Variables
    replaceAll(result, "{activeInactiveAlpha}", std::to_string(PWINDOW->m_activeInactiveAlpha->value()));
    replaceAll(result, "{alpha}", std::to_string(PWINDOW->m_alpha->value()));
    replaceAll(result, "{borderAngleAnimationProgress}", std::to_string(PWINDOW->m_borderAngleAnimationProgress->value()));
    replaceAll(result, "{borderFadeAnimationProgress}", std::to_string(PWINDOW->m_borderFadeAnimationProgress->value()));
    replaceAll(result, "{dimPercent}", std::to_string(PWINDOW->m_dimPercent->value()));
    replaceAll(result, "{movingFromWorkspaceAlpha}", std::to_string(PWINDOW->m_movingFromWorkspaceAlpha->value()));
    replaceAll(result, "{movingToWorkspaceAlpha}", std::to_string(PWINDOW->m_movingToWorkspaceAlpha->value()));
    replaceAll(result, "{notRespondingTint}", std::to_string(PWINDOW->m_notRespondingTint->value()));
    replaceAll(result, "{realPosition}", vecToStr(PWINDOW->m_realPosition->value()));
    replaceAll(result, "{realSize}", vecToStr(PWINDOW->m_realSize->value()));
    replaceAll(result, "{realShadowColor}", colorToStr(PWINDOW->m_realShadowColor->value()));
    replaceAll(result, "{monitor}", std::to_string((uintptr_t)PWINDOW->m_monitor.get()));
    replaceAll(result, "{lastCycledWindow}", std::to_string((uintptr_t)PWINDOW->m_lastCycledWindow.get()));
    replaceAll(result, "{self}", std::to_string((uintptr_t)PWINDOW->m_self.get()));
    replaceAll(result, "{swallowed}", std::to_string((uintptr_t)PWINDOW->m_swallowed.get()));
    replaceAll(result, "{workspace}", PWINDOW->m_workspace ? PWINDOW->m_workspace->m_name : "none");

    if (PWINDOW->m_pendingSizeAck.has_value()) {
        auto& val = PWINDOW->m_pendingSizeAck.value();
        replaceAll(result, "{pendingSizeAck}", std::to_string(val.first) + ":" + vecToStr(val.second));
    } else {
        replaceAll(result, "{pendingSizeAck}", "none");
    }

    //Custom Variables Based On Hyprctl Window Variables
    replaceAll(result, "{windowDecorationsCount}", std::to_string(PWINDOW->m_windowDecorations.size()));
    replaceAll(result, "{matchedRulesCount}", std::to_string(PWINDOW->m_matchedRules.size()));
    replaceAll(result, "{fullscreenInternal}", std::to_string(PWINDOW->m_fullscreenState.internal));
    replaceAll(result, "{fullscreenClient}", std::to_string(PWINDOW->m_fullscreenState.client));
    replaceAll(result, "{idleInhibitMode}", std::to_string(PWINDOW->m_idleInhibitMode));
    replaceAll(result, "{lastSurfaceMonitorID}", std::to_string(PWINDOW->m_lastSurfaceMonitorID));
    replaceAll(result, "{wantsInitialFullscreenMonitor}", std::to_string(PWINDOW->m_wantsInitialFullscreenMonitor));

    replaceAll(result, "{Date}", dateBuf);
    replaceAll(result, "{Time}", timeBuf);

    return result;
}

std::vector<std::string> splitByDelimiter(const std::string& str, const std::string& delim) {
    std::vector<std::string> out;
    size_t start = 0, end;
    while ((end = str.find(delim, start)) != std::string::npos) {
        out.push_back(str.substr(start, end - start));
        start = end + delim.length();
    }
    out.push_back(str.substr(start));
    return out;
}
CHyprBar::CHyprBar(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow) {
    hyprbars::lowlevel_log("CHyprBar::CHyprBar: enter");
    m_pWindow = pWindow;

    static auto* const PCOLOR = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_color")->getDataStaticPtr();

    const auto         PMONITOR = pWindow->m_monitor.lock();
    PMONITOR->m_scheduledRecalc = true;

    //button events
    m_pMouseButtonCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "mouseButton", [&](void* self, SCallbackInfo& info, std::any param) { onMouseButton(info, std::any_cast<IPointer::SButtonEvent>(param)); });
    m_pTouchDownCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "touchDown", [&](void* self, SCallbackInfo& info, std::any param) { onTouchDown(info, std::any_cast<ITouch::SDownEvent>(param)); });
    m_pTouchUpCallback = HyprlandAPI::registerCallbackDynamic( //
        PHANDLE, "touchUp", [&](void* self, SCallbackInfo& info, std::any param) { onTouchUp(info, std::any_cast<ITouch::SUpEvent>(param)); });

    //move events
    m_pTouchMoveCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "touchMove", [&](void* self, SCallbackInfo& info, std::any param) { onTouchMove(info, std::any_cast<ITouch::SMotionEvent>(param)); });
    m_pMouseMoveCallback = HyprlandAPI::registerCallbackDynamic( //
        PHANDLE, "mouseMove", [&](void* self, SCallbackInfo& info, std::any param) { onMouseMove(std::any_cast<Vector2D>(param)); });

    m_pTextTex    = makeShared<CTexture>();
    m_pButtonsTex = makeShared<CTexture>();

    // Initialize animated color variable via the AnimationManager so m_cRealBarColor
    // is properly created on all Hyprland versions. Use DAMAGE_NONE as a safe
    // damage flag fallback.
    g_pAnimationManager->createAnimation(CHyprColor{static_cast<uint64_t>(**PCOLOR)}, m_cRealBarColor,
                                         g_pConfigManager->getAnimationPropertyConfig("border"), pWindow, static_cast<eAVarDamagePolicy>(DAMAGE_NONE));
    m_cRealBarColor->setUpdateCallback([&](auto) { damageEntire(); });

    hyprbars::lowlevel_log("CHyprBar::CHyprBar: exit");
}

CHyprBar::~CHyprBar() {
    /* unregisterCallback is marked deprecated in newer PluginAPI.hpp; suppress deprecation
     * warnings locally until we migrate to the newer API. This keeps the build output clean.
     */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    HyprlandAPI::unregisterCallback(PHANDLE, m_pMouseButtonCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pTouchDownCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pTouchUpCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pTouchMoveCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pMouseMoveCallback);
#pragma GCC diagnostic pop
    // C++17 erase-remove idiom
    auto& bars = g_pGlobalState->bars;
    bars.erase(std::remove(bars.begin(), bars.end(), m_self), bars.end());
}

SDecorationPositioningInfo CHyprBar::getPositioningInfo() {
    static auto* const         PHEIGHT     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->getDataStaticPtr();
    static auto* const         PENABLED    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:enabled")->getDataStaticPtr();
    static auto* const         PPRECEDENCE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_precedence_over_border")->getDataStaticPtr();
    const bool localPrecedence = m_bForcedBarPrecedenceOverBorder.has_value() ? (m_bForcedBarPrecedenceOverBorder.value() != 0) : (**PPRECEDENCE != 0);

    SDecorationPositioningInfo info;
    info.policy         = m_hidden ? DECORATION_POSITION_ABSOLUTE : DECORATION_POSITION_STICKY;
    info.edges          = DECORATION_EDGE_TOP;
    info.priority       = localPrecedence ? 10005 : 5000;
    info.reserved       = true;
    info.desiredExtents = {{0, m_hidden ? 0 : m_bForcedBarHeight.value_or(**PHEIGHT)}, {0, 0}};
    return info;
}

void CHyprBar::onPositioningReply(const SDecorationPositioningReply& reply) {
    if (reply.assignedGeometry.size() != m_bAssignedBox.size())
        m_bWindowSizeChanged = true;

    m_bAssignedBox = reply.assignedGeometry;
}

std::string CHyprBar::getDisplayName() {
    return "Hyprbar";
}

bool CHyprBar::inputIsValid() {
    static auto* const PENABLED = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:enabled")->getDataStaticPtr();

    if (!**PENABLED)
        return false;

    if (!m_pWindow->m_workspace || !m_pWindow->m_workspace->isVisible() || !g_pInputManager->m_exclusiveLSes.empty() ||
        (g_pSeatManager->m_seatGrab && !g_pSeatManager->m_seatGrab->accepts(m_pWindow->m_wlSurface->resource())))
        return false;

    const auto WINDOWATCURSOR = g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

    if (WINDOWATCURSOR != m_pWindow && m_pWindow != g_pCompositor->m_lastWindow)
        return false;

    // check if input is on top or overlay shell layers
    auto     PMONITOR     = g_pCompositor->m_lastMonitor.lock();
    PHLLS    foundSurface = nullptr;
    Vector2D surfaceCoords;

    // check top layer
    g_pCompositor->vectorToLayerSurface(g_pInputManager->getMouseCoordsInternal(), &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], &surfaceCoords, &foundSurface);

    if (foundSurface)
        return false;
    // check overlay layer
    g_pCompositor->vectorToLayerSurface(g_pInputManager->getMouseCoordsInternal(), &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], &surfaceCoords,
                                        &foundSurface);

    if (foundSurface)
        return false;

    return true;
}

void CHyprBar::onMouseButton(SCallbackInfo& info, IPointer::SButtonEvent e) {
    if (!inputIsValid())
        return;

    if (e.state != WL_POINTER_BUTTON_STATE_PRESSED) {
        handleUpEvent(info);
        return;
    }

    handleDownEvent(info, std::nullopt);
}

void CHyprBar::onTouchDown(SCallbackInfo& info, ITouch::SDownEvent e) {
    // Don't do anything if you're already grabbed a window with another finger
    if (!inputIsValid() || e.touchID != 0)
        return;

    handleDownEvent(info, e);
}

void CHyprBar::onTouchUp(SCallbackInfo& info, ITouch::SUpEvent e) {
    if (!m_bDragPending || !m_bTouchEv || e.touchID != m_touchId)
        return;

    handleUpEvent(info);
}

void CHyprBar::onMouseMove(Vector2D coords) {
    // ensure proper redraws of button icons on hover when using hardware cursors
    static auto* const PICONONHOVER = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:icon_on_hover")->getDataStaticPtr();
    const bool localIconOnHover = m_bForcedIconOnHover.has_value() ? (m_bForcedIconOnHover.value() != 0) : (**PICONONHOVER != 0);
    if (localIconOnHover)
        damageOnButtonHover();

    if (!m_bDragPending || m_bTouchEv || !validMapped(m_pWindow) || m_touchId != 0)
        return;

    m_bDragPending = false;
    handleMovement();
}

void CHyprBar::onTouchMove(SCallbackInfo& info, ITouch::SMotionEvent e) {
    if (!m_bDragPending || !m_bTouchEv || !validMapped(m_pWindow) || e.touchID != m_touchId)
        return;

    auto PMONITOR     = m_pWindow->m_monitor.lock();
    PMONITOR          = PMONITOR ? PMONITOR : g_pCompositor->m_lastMonitor.lock();
    const auto COORDS = Vector2D(PMONITOR->m_position.x + e.pos.x * PMONITOR->m_size.x, PMONITOR->m_position.y + e.pos.y * PMONITOR->m_size.y);

    if (!m_bDraggingThis) {
        // Initial setup for dragging a window.
        g_pKeybindManager->m_dispatchers["setfloating"]("activewindow");
        g_pKeybindManager->m_dispatchers["resizewindowpixel"]("exact 50% 50%,activewindow");
        // pin it so you can change workspaces while dragging a window
        g_pKeybindManager->m_dispatchers["pin"]("activewindow");
    }
    {
        std::string cmd = std::string("exact ") + std::to_string((int)(COORDS.x - (assignedBoxGlobal().w / 2))) + " " + std::to_string((int)COORDS.y) + ",activewindow";
        g_pKeybindManager->m_dispatchers["movewindowpixel"](cmd);
    }
    m_bDraggingThis = true;
}

void CHyprBar::handleDownEvent(SCallbackInfo& info, std::optional<ITouch::SDownEvent> touchEvent) {
    m_bTouchEv = touchEvent.has_value();
    if (m_bTouchEv)
        m_touchId = touchEvent.value().touchID;

    const auto PWINDOW = m_pWindow.lock();

    auto       COORDS = cursorRelativeToBar();
    if (m_bTouchEv) {
        ITouch::SDownEvent e        = touchEvent.value();
        auto               PMONITOR = g_pCompositor->getMonitorFromName(!e.device->m_boundOutput.empty() ? e.device->m_boundOutput : "");
        PMONITOR                    = PMONITOR ? PMONITOR : g_pCompositor->m_lastMonitor.lock();
        COORDS = Vector2D(PMONITOR->m_position.x + e.pos.x * PMONITOR->m_size.x, PMONITOR->m_position.y + e.pos.y * PMONITOR->m_size.y) - assignedBoxGlobal().pos();
    }

    static auto* const PHEIGHT           = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->getDataStaticPtr();
    static auto* const PBARBUTTONPADDING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_button_padding")->getDataStaticPtr();
    static auto* const PBARPADDING       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_padding")->getDataStaticPtr();
    static auto* const PALIGNBUTTONS     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();
    static auto* const PONDOUBLECLICK    = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:on_double_click")->getDataStaticPtr();

    const std::string& buttonsAlign = m_bForcedBarButtonsAlignment.value_or(*PALIGNBUTTONS);
    const bool BUTTONSRIGHT    = buttonsAlign != "left";
    const std::string  ON_DOUBLE_CLICK = m_bForcedOnDoubleClick.value_or(*PONDOUBLECLICK);

    if (!VECINRECT(COORDS, 0, 0, assignedBoxGlobal().w, m_bForcedBarHeight.value_or(**PHEIGHT) - 1)) {

        if (m_bDraggingThis) {
            if (m_bTouchEv)
                g_pKeybindManager->m_dispatchers["settiled"]("activewindow");
            g_pKeybindManager->m_dispatchers["mouse"]("0movewindow");
            Debug::log(LOG, "[hyprbars] Dragging ended");
        }

        m_bDraggingThis = false;
        m_bDragPending  = false;
        m_bTouchEv      = false;
        return;
    }

    if (g_pCompositor->m_lastWindow.lock() != PWINDOW)
        g_pCompositor->focusWindow(PWINDOW);

    if (PWINDOW->m_isFloating)
        g_pCompositor->changeWindowZOrder(PWINDOW, true);

    info.cancelled   = true;
    m_bCancelledDown = true;

    if (doButtonPress(PBARPADDING, PBARBUTTONPADDING, PHEIGHT, COORDS, BUTTONSRIGHT))
        return;

    if (!ON_DOUBLE_CLICK.empty() &&
        std::chrono::duration_cast<std::chrono::milliseconds>(Time::steadyNow() - m_lastMouseDown).count() < 400 /* Arbitrary delay I found suitable */) {
        g_pKeybindManager->m_dispatchers["exec"](ON_DOUBLE_CLICK);
        m_bDragPending = false;
    } else {
        m_lastMouseDown = Time::steadyNow();
        m_bDragPending  = true;
    }
}

void CHyprBar::handleUpEvent(SCallbackInfo& info) {
    if (m_pWindow.lock() != g_pCompositor->m_lastWindow.lock())
        return;

    if (m_bCancelledDown)
        info.cancelled = true;

    m_bCancelledDown = false;

    if (m_bDraggingThis) {
        g_pKeybindManager->m_dispatchers["mouse"]("0movewindow");
        m_bDraggingThis = false;
        if (m_bTouchEv)
            g_pKeybindManager->m_dispatchers["settiled"]("activewindow");

    Debug::log(LOG, "[hyprbars] Dragging ended");
    }

    m_bDragPending = false;
    m_bTouchEv     = false;
    m_touchId      = 0;
}

void CHyprBar::handleMovement() {
    g_pKeybindManager->m_dispatchers["mouse"]("1movewindow");
    m_bDraggingThis = true;
    Debug::log(LOG, "[hyprbars] Dragging initiated");
    return;
}

bool CHyprBar::doButtonPress(Hyprlang::INT* const* PBARPADDING, Hyprlang::INT* const* PBARBUTTONPADDING, Hyprlang::INT* const* PHEIGHT, Vector2D COORDS, const bool BUTTONSRIGHT) {
    //check if on a button
    float offset = m_bForcedBarPadding.value_or(**PBARPADDING);

    if (!m_windowRuleButtons.empty()) {
        for (auto& b : m_windowRuleButtons) {
            const auto BARBUF     = Vector2D{(int)assignedBoxGlobal().w, m_bForcedBarHeight.value_or(**PHEIGHT)};
            Vector2D   currentPos = Vector2D{(BUTTONSRIGHT ? BARBUF.x - m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) - b.size - offset : offset), (BARBUF.y - b.size) / 2.0}.floor();

            if (VECINRECT(COORDS, currentPos.x, currentPos.y, currentPos.x + b.size + m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING), currentPos.y + b.size)) {
                // hit on close
                g_pKeybindManager->m_dispatchers["exec"](b.cmd);
                return true;
            }

            offset += m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) + b.size;
        }
    } else {
        for (auto& b : g_pGlobalState->buttons) {
            const auto BARBUF     = Vector2D{(int)assignedBoxGlobal().w, m_bForcedBarHeight.value_or(**PHEIGHT)};
            Vector2D   currentPos = Vector2D{(BUTTONSRIGHT ? BARBUF.x - m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) - b.size - offset : offset), (BARBUF.y - b.size) / 2.0}.floor();

            if (VECINRECT(COORDS, currentPos.x, currentPos.y, currentPos.x + b.size + m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING), currentPos.y + b.size)) {
                // hit on close
                g_pKeybindManager->m_dispatchers["exec"](b.cmd);
                return true;
            }

            offset += m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) + b.size;
        }
    }
    return false;
}

bool CHyprBar::createTextureFromCairoSurface(SP<CTexture> out, cairo_surface_t* surface, cairo_t* cr, const Vector2D& bufferSize, const char* debugName) {
    // Flush the cairo surface to ensure all drawing is complete
    cairo_surface_flush(surface);

    // Get the pixel data
    const auto DATA = cairo_image_surface_get_data(surface);
    if (!DATA) {
        char dbg[256];
        snprintf(dbg, sizeof(dbg), "%s: failed to get cairo surface data", debugName);
        hyprbars::lowlevel_log(dbg);
        return false;
    }

    // Allocate the GL texture
    out->allocate();
    if (out->m_texID == 0) {
        char dbg[256];
        snprintf(dbg, sizeof(dbg), "%s: texture allocation failed", debugName);
        hyprbars::lowlevel_log(dbg);
        return false;
    }

    char dbg[256];
    snprintf(dbg, sizeof(dbg), "%s: binding texture %u", debugName, (unsigned)out->m_texID);
    hyprbars::lowlevel_log(dbg);

    // Set up the texture parameters
    glBindTexture(GL_TEXTURE_2D, out->m_texID);
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        snprintf(dbg, sizeof(dbg), "%s: glBindTexture failed with error 0x%x", debugName, error);
        hyprbars::lowlevel_log(dbg);
        return false;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

#ifndef GLES2
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
#endif

    // Upload the texture data
    snprintf(dbg, sizeof(dbg), "%s: uploading texture data", debugName);
    hyprbars::lowlevel_log(dbg);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bufferSize.x, bufferSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    error = glGetError();
    if (error != GL_NO_ERROR) {
        snprintf(dbg, sizeof(dbg), "%s: glTexImage2D failed with error 0x%x", debugName, error);
        hyprbars::lowlevel_log(dbg);
        return false;
    }

    snprintf(dbg, sizeof(dbg), "%s: texture created successfully, id=%u size=%dx%d",
             debugName, (unsigned)out->m_texID, (int)bufferSize.x, (int)bufferSize.y);
    hyprbars::lowlevel_log(dbg);

    return true;
}

void CHyprBar::renderText(SP<CTexture> out, const std::string& text, const CHyprColor& color, const Vector2D& bufferSize, const float scale, const int fontSize) {
    hyprbars::lowlevel_log("renderText: enter");
    char dbg[256];
    snprintf(dbg, sizeof(dbg), "renderText: bufferSize=%dx%d text='%s' fontSize=%d",
             (int)bufferSize.x, (int)bufferSize.y, text.c_str(), fontSize);
    hyprbars::lowlevel_log(dbg);

    const auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);
    if (cairo_surface_status(CAIROSURFACE) != CAIRO_STATUS_SUCCESS) {
        hyprbars::lowlevel_log("renderText: failed to create cairo surface");
        return;
    }

    const auto CAIRO = cairo_create(CAIROSURFACE);
    if (cairo_status(CAIRO) != CAIRO_STATUS_SUCCESS) {
        hyprbars::lowlevel_log("renderText: failed to create cairo context");
        cairo_surface_destroy(CAIROSURFACE);
        return;
    }

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    // draw title using Pango
    PangoLayout* layout = pango_cairo_create_layout(CAIRO);
    pango_layout_set_text(layout, text.c_str(), -1);

    PangoFontDescription* fontDesc = pango_font_description_from_string("sans");
    pango_font_description_set_size(fontDesc, fontSize * scale * PANGO_SCALE);
    pango_layout_set_font_description(layout, fontDesc);
    pango_font_description_free(fontDesc);

    const int maxWidth = bufferSize.x;

    pango_layout_set_width(layout, maxWidth * PANGO_SCALE);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);

    cairo_set_source_rgba(CAIRO, color.r, color.g, color.b, color.a);

    PangoRectangle ink_rect, logical_rect;
    pango_layout_get_extents(layout, &ink_rect, &logical_rect);

    const int    layoutWidth  = ink_rect.width;
    const int    layoutHeight = logical_rect.height;

    const double xOffset = (bufferSize.x / 2.0 - layoutWidth / PANGO_SCALE / 2.0);
    const double yOffset = (bufferSize.y / 2.0 - layoutHeight / PANGO_SCALE / 2.0);

    cairo_move_to(CAIRO, xOffset, yOffset);
    pango_cairo_show_layout(CAIRO, layout);

    g_object_unref(layout);

    if (!createTextureFromCairoSurface(out, CAIROSURFACE, CAIRO, bufferSize, "renderText")) {
        cairo_destroy(CAIRO);
        cairo_surface_destroy(CAIROSURFACE);
        return;
    }

    // delete cairo resources
    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);
}

void CHyprBar::renderBarTitle(const Vector2D& bufferSize, const float scale) {
    static auto* const PCOLOR            = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:col.text")->getDataStaticPtr();
    static auto* const PSIZE             = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_text_size")->getDataStaticPtr();
    static auto* const PFONT             = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_text_font")->getDataStaticPtr();
    static auto* const PALIGN            = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_text_align")->getDataStaticPtr();
    static auto* const PALIGNBUTTONS     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();
    static auto* const PBARPADDING       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_padding")->getDataStaticPtr();
    static auto* const PBARBUTTONPADDING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_button_padding")->getDataStaticPtr();

    const std::string& buttonsAlign = m_bForcedBarButtonsAlignment.value_or(*PALIGNBUTTONS);
    const bool BUTTONSRIGHT = buttonsAlign != "left";

    const auto         PWINDOW = m_pWindow.lock();

    const auto         BORDERSIZE = PWINDOW->getRealBorderSize();

    float              buttonSizes = m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING);
    if (!m_windowRuleButtons.empty()) {
        for (auto& b : m_windowRuleButtons) {
            buttonSizes += b.size + m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING);
        }
    } else {
        for (const auto& b : g_pGlobalState->buttons) {
            buttonSizes += b.size + m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING);
        }
    }
    const auto scaledSize = (m_bForcedBarTextSize.value_or(**PSIZE)) * scale;
    const auto       scaledBorderSize  = BORDERSIZE * scale;
    const auto       scaledButtonsSize = buttonSizes * scale;
    const auto       scaledButtonsPad  = m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) * scale;
    const auto       scaledBarPadding  = m_bForcedBarPadding.value_or(**PBARPADDING) * scale;

    const CHyprColor COLOR = m_bForcedTitleColor.value_or(**PCOLOR);

    const auto       CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);
    const auto       CAIRO        = cairo_create(CAIROSURFACE);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    // draw title using Pango
    PangoLayout* layout = pango_cairo_create_layout(CAIRO);
    pango_layout_set_text(layout, m_szLastTitle.c_str(), -1);

    const std::string& fontName = m_bForcedBarTextFont.value_or(*PFONT);
    PangoFontDescription* fontDesc = pango_font_description_from_string(fontName.c_str());
    pango_font_description_set_size(fontDesc, scaledSize * PANGO_SCALE);
    pango_layout_set_font_description(layout, fontDesc);
    pango_font_description_free(fontDesc);

    PangoContext* context = pango_layout_get_context(layout);
    pango_context_set_base_dir(context, PANGO_DIRECTION_NEUTRAL);

    // --- Use forced alignment if present ---
    const std::string& align = m_bForcedBarTextAlign.value_or(*PALIGN);

    const int paddingTotal = scaledBarPadding * 2 + scaledButtonsSize + (std::string{*PALIGN} != "left" ? scaledButtonsSize : 0);
    const int maxWidth     = std::clamp(static_cast<int>(bufferSize.x - paddingTotal), 0, std::numeric_limits<int>::max());

    pango_layout_set_width(layout, maxWidth * PANGO_SCALE);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

    cairo_set_source_rgba(CAIRO, COLOR.r, COLOR.g, COLOR.b, COLOR.a);

    int layoutWidth, layoutHeight;
    pango_layout_get_size(layout, &layoutWidth, &layoutHeight);
    int xOffset = 0;
    if (align == "left") {
        xOffset = std::round(scaledBarPadding + (BUTTONSRIGHT ? 0 : scaledButtonsSize));
    } else if (align == "center") {
        xOffset = std::round(((bufferSize.x - scaledBorderSize) / 2.0 - layoutWidth / PANGO_SCALE / 2.0));
    } else if (align == "right") {
        xOffset = std::round(bufferSize.x - scaledBarPadding - layoutWidth / PANGO_SCALE - (BUTTONSRIGHT ? scaledButtonsSize : 0));
    } else {
        // fallback to center
        xOffset = std::round(((bufferSize.x - scaledBorderSize) / 2.0 - layoutWidth / PANGO_SCALE / 2.0));
    }
    const int yOffset = std::round((bufferSize.y / 2.0 - layoutHeight / PANGO_SCALE / 2.0));

    cairo_move_to(CAIRO, xOffset, yOffset);
    pango_cairo_show_layout(CAIRO, layout);

    g_object_unref(layout);

    if (!createTextureFromCairoSurface(m_pTextTex, CAIROSURFACE, CAIRO, bufferSize, "renderBarTitle")) {
        cairo_destroy(CAIRO);
        cairo_surface_destroy(CAIROSURFACE);
        return;
    }

    // delete cairo resources
    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);
}

size_t CHyprBar::getVisibleButtonCount(Hyprlang::INT* const* PBARBUTTONPADDING, Hyprlang::INT* const* PBARPADDING, const Vector2D& bufferSize, const float scale) {
    float  availableSpace = bufferSize.x - m_bForcedBarPadding.value_or(**PBARPADDING) * scale * 2;
    size_t count          = 0;

    if (!m_windowRuleButtons.empty()) {
        for (auto& button : m_windowRuleButtons) {
            const float buttonSpace = (button.size + m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING)) * scale;
            if (availableSpace >= buttonSpace) {
                count++;
                availableSpace -= buttonSpace;
            } else
                break;
        }
    } else {
        for (const auto& button : g_pGlobalState->buttons) {
            const float buttonSpace = (button.size + m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING)) * scale;
            if (availableSpace >= buttonSpace) {
                count++;
                availableSpace -= buttonSpace;
            } else
                break;
        }
    }

    return count;
}

void CHyprBar::renderBarButtons(const Vector2D& bufferSize, const float scale) {
    static auto* const PBARBUTTONPADDING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_button_padding")->getDataStaticPtr();
    static auto* const PBARPADDING       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_padding")->getDataStaticPtr();
    static auto* const PALIGNBUTTONS     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();
    CHyprColor inactiveColor;
    if (m_bForcedInactiveButtonColor.has_value()) {
        inactiveColor = m_bForcedInactiveButtonColor.value();
    } else {
        auto* const PINACTIVECOLOR = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:inactive_button_color")->getDataStaticPtr();
        inactiveColor = CHyprColor(**PINACTIVECOLOR);
    }

    const std::string& buttonsAlign = m_bForcedBarButtonsAlignment.value_or(*PALIGNBUTTONS);
    const bool BUTTONSRIGHT = buttonsAlign != "left";
    const auto         visibleCount = getVisibleButtonCount(PBARBUTTONPADDING, PBARPADDING, bufferSize, scale);

    const auto         CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);
    const auto         CAIRO        = cairo_create(CAIROSURFACE);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    // draw buttons
    int offset = m_bForcedBarPadding.value_or(**PBARPADDING) * scale;
    if (!m_windowRuleButtons.empty()) {
        for (size_t i = 0; i < visibleCount && i < m_windowRuleButtons.size(); ++i) {
            const auto& button = m_windowRuleButtons[i];
            const auto  scaledButtonSize = button.size * scale;
            const auto  scaledButtonsPad = m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) * scale;

        const auto  pos   = Vector2D{BUTTONSRIGHT ? bufferSize.x - offset - scaledButtonSize / 2.0 : offset + scaledButtonSize / 2.0, bufferSize.y / 2.0}.floor();
        auto        color = button.bgcol;

        if (inactiveColor.a > 0.0f) {
            color = m_bWindowHasFocus ? color : inactiveColor;
            if (button.userfg && button.iconTex->m_texID != 0)
                button.iconTex->destroyTexture();
        }

        cairo_set_source_rgba(CAIRO, color.r, color.g, color.b, color.a);
        cairo_arc(CAIRO, pos.x, pos.y, scaledButtonSize / 2, 0, 2 * M_PI);
        cairo_fill(CAIRO);

            offset += scaledButtonsPad + scaledButtonSize;
        }
    } else {
        for (size_t i = 0; i < visibleCount && i < g_pGlobalState->buttons.size(); ++i) {
            const auto& button = g_pGlobalState->buttons[i];
            const auto  scaledButtonSize = button.size * scale;
            const auto  scaledButtonsPad = m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) * scale;

            const auto  pos = Vector2D{BUTTONSRIGHT ? bufferSize.x - offset - scaledButtonSize / 2.0 : offset + scaledButtonSize / 2.0, bufferSize.y / 2.0}.floor();

            cairo_set_source_rgba(CAIRO, button.bgcol.r, button.bgcol.g, button.bgcol.b, button.bgcol.a);
            cairo_arc(CAIRO, pos.x, pos.y, scaledButtonSize / 2, 0, 2 * M_PI);
            cairo_fill(CAIRO);

            offset += scaledButtonsPad + scaledButtonSize;
        }
    }

    if (!createTextureFromCairoSurface(m_pButtonsTex, CAIROSURFACE, CAIRO, bufferSize, "renderBarButtons")) {
        cairo_destroy(CAIRO);
        cairo_surface_destroy(CAIROSURFACE);
        return;
    }

    // delete cairo resources
    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);
}

void CHyprBar::renderBarButtonsText(CBox* barBox, const float scale, const float a) {
    static auto* const PHEIGHT           = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->getDataStaticPtr();
    static auto* const PBARBUTTONPADDING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_button_padding")->getDataStaticPtr();
    static auto* const PBARPADDING       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_padding")->getDataStaticPtr();
    static auto* const PALIGNBUTTONS     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();
    static auto* const PICONONHOVER      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:icon_on_hover")->getDataStaticPtr();
    const bool localIconOnHover = m_bForcedIconOnHover.has_value() ? (m_bForcedIconOnHover.value() != 0) : (**PICONONHOVER != 0);

    const std::string& buttonsAlign = m_bForcedBarButtonsAlignment.value_or(*PALIGNBUTTONS);
    const bool BUTTONSRIGHT = buttonsAlign != "left";
    const auto         visibleCount = getVisibleButtonCount(PBARBUTTONPADDING, PBARPADDING, Vector2D{barBox->w, barBox->h}, scale);
    const auto         COORDS       = cursorRelativeToBar();

    int                offset        = m_bForcedBarPadding.value_or(**PBARPADDING) * scale;
    float              noScaleOffset = m_bForcedBarPadding.value_or(**PBARPADDING);

    if (!m_windowRuleButtons.empty()) {
        for (size_t i = 0; i < visibleCount && i < m_windowRuleButtons.size(); ++i) {
            const auto& button = m_windowRuleButtons[i];
            const auto scaledButtonSize = button.size * scale;
            const auto scaledButtonsPad = m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) * scale;

            // check if hovering here
            const auto BARBUF     = Vector2D{(int)assignedBoxGlobal().w, m_bForcedBarHeight.value_or(**PHEIGHT)};
            Vector2D   currentPos = Vector2D{(BUTTONSRIGHT ? BARBUF.x - m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) - button.size - noScaleOffset : noScaleOffset), (BARBUF.y - button.size) / 2.0}.floor();
            bool       hovering   = VECINRECT(COORDS, currentPos.x, currentPos.y, currentPos.x + button.size + m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING), currentPos.y + button.size);
            noScaleOffset += m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) + button.size;

            if (button.iconTex->m_texID == 0 /* icon is not rendered */ && !button.icon.empty()) {
                // render icon
                const Vector2D BUFSIZE = {scaledButtonSize, scaledButtonSize};
                CHyprColor fgcol = (button.userfg && button.fgcol.has_value()) ? button.fgcol.value() : ((button.bgcol.r + button.bgcol.g + button.bgcol.b < 1) ? CHyprColor(0xFFFFFFFF) : CHyprColor(0xFF000000));

                renderText(button.iconTex, button.icon, fgcol, BUFSIZE, scale, button.size * 0.62);
            }

            if (button.iconTex->m_texID == 0)
                continue;

            CBox pos = {barBox->x + (BUTTONSRIGHT ? barBox->width - offset - scaledButtonSize : offset), barBox->y + (barBox->height - scaledButtonSize) / 2.0, scaledButtonSize,
                        scaledButtonSize};

            if (!localIconOnHover || (localIconOnHover && m_iButtonHoverState > 0)) {
#ifdef HYPRLAND_049
                g_pHyprOpenGL->renderTexture(button.iconTex, pos, a);
#else
                CHyprOpenGLImpl::STextureRenderData texData = {};
                texData.a = a;
                g_pHyprOpenGL->renderTexture(button.iconTex, pos, texData);
#endif
            }
            offset += scaledButtonsPad + scaledButtonSize;

            bool currentBit = (m_iButtonHoverState & (1 << i)) != 0;
            if (hovering != currentBit) {
                m_iButtonHoverState ^= (1 << i);
                // damage to get rid of some artifacts when icons are "hidden"
                damageEntire();
            }
        }
    } else {
        for (size_t i = 0; i < visibleCount && i < g_pGlobalState->buttons.size(); ++i) {
            const auto& button = g_pGlobalState->buttons[i];
            const auto scaledButtonSize = button.size * scale;
            const auto scaledButtonsPad = m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) * scale;

            // check if hovering here
            const auto BARBUF     = Vector2D{(int)assignedBoxGlobal().w, m_bForcedBarHeight.value_or(**PHEIGHT)};
            Vector2D   currentPos = Vector2D{(BUTTONSRIGHT ? BARBUF.x - m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) - button.size - noScaleOffset : noScaleOffset), (BARBUF.y - button.size) / 2.0}.floor();
            bool       hovering   = VECINRECT(COORDS, currentPos.x, currentPos.y, currentPos.x + button.size + m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING), currentPos.y + button.size);
            noScaleOffset += m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) + button.size;

            if (button.iconTex->m_texID == 0 /* icon is not rendered */ && !button.icon.empty()) {
                // render icon
                const Vector2D BUFSIZE = {scaledButtonSize, scaledButtonSize};
                auto           fgcol   = button.userfg ? button.fgcol : (button.bgcol.r + button.bgcol.g + button.bgcol.b < 1) ? CHyprColor(0xFFFFFFFF) : CHyprColor(0xFF000000);

                renderText(button.iconTex, button.icon, fgcol, BUFSIZE, scale, button.size * 0.62);
            }

            if (button.iconTex->m_texID == 0)
                continue;

            CBox pos = {barBox->x + (BUTTONSRIGHT ? barBox->width - offset - scaledButtonSize : offset), barBox->y + (barBox->height - scaledButtonSize) / 2.0, scaledButtonSize,
                        scaledButtonSize};

            if (!localIconOnHover || (localIconOnHover && m_iButtonHoverState > 0)) {
#ifdef HYPRLAND_049
                g_pHyprOpenGL->renderTexture(button.iconTex, pos, a);
#else
                CHyprOpenGLImpl::STextureRenderData texData = {};
                texData.a = a;
                g_pHyprOpenGL->renderTexture(button.iconTex, pos, texData);
#endif
            }
            offset += scaledButtonsPad + scaledButtonSize;

            bool currentBit = (m_iButtonHoverState & (1 << i)) != 0;
            if (hovering != currentBit) {
                m_iButtonHoverState ^= (1 << i);
                // damage to get rid of some artifacts when icons are "hidden"
                damageEntire();
            }
        }
    }
}

void CHyprBar::draw(PHLMONITOR pMonitor, const float& a) {
    static auto* const PENABLED = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:enabled")->getDataStaticPtr();

    if (m_bLastEnabledState != **PENABLED) {
        m_bLastEnabledState = **PENABLED;
        g_pDecorationPositioner->repositionDeco(this);
    }

    // Diagnostic: log draw entry and key state
    {
        static bool logged_initial_state = false;
        const auto PWINDOW = m_pWindow.lock();

        
        // Log when textures are missing but window is ready for display
        if (!m_hidden && validMapped(m_pWindow) && **PENABLED && PWINDOW && PWINDOW->m_isMapped) {
            if (!m_pTextTex || m_pTextTex->m_texID == 0 || !m_pButtonsTex || m_pButtonsTex->m_texID == 0) {
                char dbg[256];
                snprintf(dbg, sizeof(dbg), "draw: textures missing but window ready: texText=%u texButtons=%u",
                         (unsigned)(m_pTextTex ? m_pTextTex->m_texID : 0), 
                         (unsigned)(m_pButtonsTex ? m_pButtonsTex->m_texID : 0));
                hyprbars::lowlevel_log(dbg);
            }
        }
    }

    if (m_hidden || !validMapped(m_pWindow) || !**PENABLED)
        return;

    const auto PWINDOW = m_pWindow.lock();

    if (!PWINDOW->m_windowData.decorate.valueOrDefault())
        return;


    auto data = CBarPassElement::SBarData{this, a};
#ifdef HYPRLAND_049
    g_pHyprRenderer->m_renderPass.add(makeShared<CBarPassElement>(data));
#else
    g_pHyprRenderer->m_renderPass.add(makeUnique<CBarPassElement>(data));
#endif
    // DEBUG: also call renderPass directly to confirm render code path is valid
    hyprbars::lowlevel_log("draw: DEBUG calling renderPass directly");
    try {
        renderPass(pMonitor, a);
    } catch (...) {
        hyprbars::lowlevel_log("draw: DEBUG direct renderPass threw");
    }
}

void CHyprBar::renderPass(PHLMONITOR pMonitor, const float& a) {
    hyprbars::lowlevel_log("CHyprBar::renderPass: enter");
    const auto         PWINDOW = m_pWindow.lock();

    static auto* const PCOLOR            = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_color")->getDataStaticPtr();
    static auto* const PHEIGHT           = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->getDataStaticPtr();
    static auto* const PPRECEDENCE       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_precedence_over_border")->getDataStaticPtr();
    const bool localPrecedence = m_bForcedBarPrecedenceOverBorder.has_value() ? (m_bForcedBarPrecedenceOverBorder.value() != 0) : (**PPRECEDENCE != 0);
    static auto* const PALIGNBUTTONS     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();
    static auto* const PENABLETITLE      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_title_enabled")->getDataStaticPtr();
    const bool localTitleEnabled = m_bForcedBarTitleEnabled.has_value() ? (m_bForcedBarTitleEnabled.value() != 0) : (**PENABLETITLE != 0);
    static auto* const PENABLEBLUR       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_blur")->getDataStaticPtr();
    const bool localBlur = m_bForcedBarBlur.has_value() ? (m_bForcedBarBlur.value() != 0) : (**PENABLEBLUR != 0);
    static auto* const PENABLEBLURGLOBAL = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "decoration:blur:enabled")->getDataStaticPtr();
    CHyprColor inactiveColor;
    if (m_bForcedInactiveButtonColor.has_value()) {
        inactiveColor = m_bForcedInactiveButtonColor.value();
    } else {
        auto* const PINACTIVECOLOR = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:inactive_button_color")->getDataStaticPtr();
        inactiveColor = CHyprColor(**PINACTIVECOLOR);
    }

    if (inactiveColor.a > 0.0f) {
        bool currentWindowFocus = PWINDOW == g_pCompositor->m_lastWindow.lock();
        if (currentWindowFocus != m_bWindowHasFocus) {
            // Declare all needed variables at the top of the function
            const auto         PWINDOW = m_pWindow.lock();
            static auto* const PCOLOR            = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_color")->getDataStaticPtr();
            static auto* const PHEIGHT           = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->getDataStaticPtr();
            static auto* const PPRECEDENCE       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_precedence_over_border")->getDataStaticPtr();
            const bool localPrecedence = m_bForcedBarPrecedenceOverBorder.has_value() ? (m_bForcedBarPrecedenceOverBorder.value() != 0) : (**PPRECEDENCE != 0);
            static auto* const PALIGNBUTTONS     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();
            static auto* const PENABLETITLE      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_title_enabled")->getDataStaticPtr();
            const bool localTitleEnabled = m_bForcedBarTitleEnabled.has_value() ? (m_bForcedBarTitleEnabled.value() != 0) : (**PENABLETITLE != 0);
            static auto* const PENABLEBLUR       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_blur")->getDataStaticPtr();
            const bool localBlur = m_bForcedBarBlur.has_value() ? (m_bForcedBarBlur.value() != 0) : (**PENABLEBLUR != 0);
            static auto* const PENABLEBLURGLOBAL = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "decoration:blur:enabled")->getDataStaticPtr();
            CHyprColor inactiveColor;
            if (m_bForcedInactiveButtonColor.has_value()) {
                inactiveColor = m_bForcedInactiveButtonColor.value();
            } else {
                auto* const PINACTIVECOLOR = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:inactive_button_color")->getDataStaticPtr();
                inactiveColor = CHyprColor(**PINACTIVECOLOR);
            }
            const CHyprColor DEST_COLOR = m_bForcedBarColor.value_or(**PCOLOR);
            if (DEST_COLOR != m_cRealBarColor->goal())
                *m_cRealBarColor = DEST_COLOR;
            CHyprColor color = m_cRealBarColor->value();
            color.a *= a;
            const std::string& buttonsAlign = m_bForcedBarButtonsAlignment.value_or(*PALIGNBUTTONS);
            const bool BUTTONSRIGHT = buttonsAlign != "left";
            const bool SHOULDBLUR   = localBlur && **PENABLEBLURGLOBAL && color.a < 1.F;
            if (m_bForcedBarHeight.value_or(**PHEIGHT) < 1) {
                m_iLastHeight = m_bForcedBarHeight.value_or(**PHEIGHT);
                return;
            }
            const auto PWORKSPACE      = PWINDOW->m_workspace;
            const auto WORKSPACEOFFSET = PWORKSPACE && !PWINDOW->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D();
            const auto ROUNDING = PWINDOW->rounding() + (localPrecedence ? 0 : PWINDOW->getRealBorderSize());
            const auto scaledRounding = ROUNDING > 0 ? ROUNDING * pMonitor->m_scale - 2 : 0;
            m_seExtents = {{0, m_bForcedBarHeight.value_or(**PHEIGHT)}, {}};
            const auto DECOBOX = assignedBoxGlobal();
            const auto BARBUF = DECOBOX.size() * pMonitor->m_scale;
            CBox       titleBarBox = {DECOBOX.x - pMonitor->m_position.x, DECOBOX.y - pMonitor->m_position.y, DECOBOX.w,
                                      DECOBOX.h + ROUNDING * 3};
            titleBarBox.translate(PWINDOW->m_floatingOffset).scale(pMonitor->m_scale).round();
            if (titleBarBox.w < 1 || titleBarBox.h < 1)
                return;
            g_pHyprOpenGL->scissor(titleBarBox);
            if (ROUNDING) {
                // the +1 is a shit garbage temp fix until renderRect supports an alpha matte
                CBox windowBox = {PWINDOW->m_realPosition->value().x + PWINDOW->m_floatingOffset.x - pMonitor->m_position.x + 1,
                                  PWINDOW->m_realPosition->value().y + PWINDOW->m_floatingOffset.y - pMonitor->m_position.y + 1, PWINDOW->m_realSize->value().x - 2,
                                  PWINDOW->m_realSize->value().y - 2};

                if (windowBox.w < 1 || windowBox.h < 1)
                    return;

                glClearStencil(0);
                glClear(GL_STENCIL_BUFFER_BIT);
        #ifdef HYPRLAND_049
        #else
                g_pHyprOpenGL->setCapStatus(GL_STENCIL_TEST, true);
        #endif
                glStencilFunc(GL_ALWAYS, 1, -1);
                glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

            windowBox.translate(WORKSPACEOFFSET).scale(pMonitor->m_scale).round();
        #ifdef HYPRLAND_049
            g_pHyprOpenGL->renderRect(windowBox, CHyprColor(0, 0, 0, 0), scaledRounding, m_pWindow->roundingPower());
        #else
            CHyprOpenGLImpl::SRectRenderData rectData = {};
            rectData.round = scaledRounding;
            rectData.roundingPower = m_pWindow->roundingPower();
            g_pHyprOpenGL->renderRect(windowBox, CHyprColor(0, 0, 0, 0), rectData);
        #endif
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

                glStencilFunc(GL_NOTEQUAL, 1, -1);
                glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            }

            if (SHOULDBLUR) {
        #ifdef HYPRLAND_049
                g_pHyprOpenGL->renderRect(titleBarBox, color, scaledRounding, m_pWindow->roundingPower());
        #else
            CHyprOpenGLImpl::SRectRenderData rectData = {};
            rectData.round = scaledRounding;
            rectData.roundingPower = m_pWindow->roundingPower();
            g_pHyprOpenGL->renderRect(titleBarBox, color, rectData);
        #endif
            }
        #ifdef HYPRLAND_049
            g_pHyprOpenGL->renderRect(titleBarBox, color, scaledRounding, m_pWindow->roundingPower());
        #else
            CHyprOpenGLImpl::SRectRenderData rectData = {};
            rectData.round = scaledRounding;
            rectData.roundingPower = m_pWindow->roundingPower();
            g_pHyprOpenGL->renderRect(titleBarBox, color, rectData);
        #endif

            // render title
            int currentTextSize = m_bForcedBarTextSize.value_or(**((Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_text_size")->getDataStaticPtr()));
            if (localTitleEnabled && (m_szLastTitle != PWINDOW->m_title || m_bWindowSizeChanged || m_pTextTex->m_texID == 0 || m_bTitleColorChanged || m_iLastTextSize != currentTextSize)) {
                if (m_bForcedBarCustomTitle.has_value()) {
                    m_szLastTitle = substituteTitleVars(m_bForcedBarCustomTitle.value(), PWINDOW);
                } else {
                    m_szLastTitle = PWINDOW->m_title;
                }
                renderBarTitle(BARBUF, pMonitor->m_scale);
                m_iLastTextSize = currentTextSize;
            }

            if (ROUNDING) {
                // cleanup stencil
                glClearStencil(0);
                glClear(GL_STENCIL_BUFFER_BIT);
#ifndef HYPRLAND_049
                g_pHyprOpenGL->setCapStatus(GL_STENCIL_TEST, false);
#else
                glDisable(GL_STENCIL_TEST);
#endif
                glStencilMask(-1);
                glStencilFunc(GL_ALWAYS, 1, 0xFF);
            }

            CBox textBox = {titleBarBox.x, titleBarBox.y, (int)BARBUF.x, (int)BARBUF.y};

            // runtime diagnostic: log BARBUF, texture ids, title length and whether title rendering is enabled
            {
                char dbg[512];
                snprintf(dbg, sizeof(dbg), "renderPass: BARBUF=%dx%d m_pTextTex=%u m_pButtonsTex=%u title_len=%zu enabled=%d color=%.3f,%.3f,%.3f,%.3f",
                         (int)BARBUF.x, (int)BARBUF.y, (unsigned)(m_pTextTex ? m_pTextTex->m_texID : 0), (unsigned)(m_pButtonsTex ? m_pButtonsTex->m_texID : 0), m_szLastTitle.size(), localTitleEnabled ? 1 : 0,
                         color.r, color.g, color.b, color.a);
                hyprbars::lowlevel_log(dbg);
            }
            if (localTitleEnabled) {
                // If the texture wasn't created for whatever reason, draw a
                // visible debug rectangle so we can confirm the render path
                // is being executed and isolate the missing-texture problem.
                if (m_pTextTex && m_pTextTex->m_texID == 0) {
                    hyprbars::lowlevel_log("renderPass: m_pTextTex == 0, drawing debug rect fallback");
#ifdef HYPRLAND_049
                    g_pHyprOpenGL->renderRect(textBox, CHyprColor(1.0, 0.0, 1.0, 0.85), scaledRounding, m_pWindow->roundingPower());
#else
                    CHyprOpenGLImpl::SRectRenderData debugRect = {};
                    debugRect.round = scaledRounding;
                    debugRect.roundingPower = m_pWindow->roundingPower();
                    g_pHyprOpenGL->renderRect(textBox, CHyprColor(1.0, 0.0, 1.0, 0.85), debugRect);
#endif
                } else {
#ifdef HYPRLAND_049
                    g_pHyprOpenGL->renderTexture(m_pTextTex, textBox, a);
#else
                    CHyprOpenGLImpl::STextureRenderData textTexData = {};
                    textTexData.a = a;
                    g_pHyprOpenGL->renderTexture(m_pTextTex, textBox, textTexData);
#endif
                }
            }

            if (m_bButtonsDirty || m_bWindowSizeChanged) {
                renderBarButtons(BARBUF, pMonitor->m_scale);
                m_bButtonsDirty = false;
            }

            // If buttons texture is missing, log and let renderBarButtons have
            // already attempted to create it. If it's still missing, draw a
            // small debug rect aligned to the right so we can see the area.
            if (m_pButtonsTex && m_pButtonsTex->m_texID == 0) {
                hyprbars::lowlevel_log("renderPass: m_pButtonsTex == 0, drawing debug rect fallback for buttons");
                // draw a thin magenta bar on the right side of the textBox
                CBox dbgBtnBox = textBox;
                dbgBtnBox.x = textBox.x + textBox.width - 20;
                dbgBtnBox.width = 20;
#ifdef HYPRLAND_049
                g_pHyprOpenGL->renderRect(dbgBtnBox, CHyprColor(1.0, 0.0, 1.0, 0.85), scaledRounding, m_pWindow->roundingPower());
#else
                CHyprOpenGLImpl::SRectRenderData debugRect = {};
                debugRect.round = scaledRounding;
                debugRect.roundingPower = m_pWindow->roundingPower();
                g_pHyprOpenGL->renderRect(dbgBtnBox, CHyprColor(1.0, 0.0, 1.0, 0.85), debugRect);
#endif
            } else {
#ifdef HYPRLAND_049
                g_pHyprOpenGL->renderTexture(m_pButtonsTex, textBox, a);
#else
                CHyprOpenGLImpl::STextureRenderData btnTexData = {};
                btnTexData.a = a;
                g_pHyprOpenGL->renderTexture(m_pButtonsTex, textBox, btnTexData);
#endif
            }

            g_pHyprOpenGL->scissor(nullptr);

            renderBarButtonsText(&textBox, pMonitor->m_scale, a);

            m_bWindowSizeChanged = false;
            m_bTitleColorChanged = false;

            // dynamic updates change the extents
            if (m_iLastHeight != m_bForcedBarHeight.value_or(**PHEIGHT)) {
                g_pLayoutManager->getCurrentLayout()->recalculateWindow(PWINDOW);
                m_iLastHeight = m_bForcedBarHeight.value_or(**PHEIGHT);
            }
        }
    }
}


eDecorationType CHyprBar::getDecorationType() {
    return DECORATION_CUSTOM;
}

void CHyprBar::updateWindow(PHLWINDOW pWindow) {
    damageEntire();
}

void CHyprBar::damageEntire() {
    g_pHyprRenderer->damageBox(assignedBoxGlobal());
}

Vector2D CHyprBar::cursorRelativeToBar() {
    return g_pInputManager->getMouseCoordsInternal() - assignedBoxGlobal().pos();
}

eDecorationLayer CHyprBar::getDecorationLayer() {
    return DECORATION_LAYER_UNDER;
}

uint64_t CHyprBar::getDecorationFlags() {
    static auto* const PPART = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_part_of_window")->getDataStaticPtr();
    const bool localPartOfWindow = m_bForcedBarPartOfWindow.has_value() ? (m_bForcedBarPartOfWindow.value() != 0) : (**PPART != 0);
    return DECORATION_ALLOWS_MOUSE_INPUT | (localPartOfWindow ? DECORATION_PART_OF_MAIN_WINDOW : 0);
}

CBox CHyprBar::assignedBoxGlobal() {
    if (!validMapped(m_pWindow))
        return {};

    CBox box = m_bAssignedBox;
    box.translate(g_pDecorationPositioner->getEdgeDefinedPoint(DECORATION_EDGE_TOP, m_pWindow.lock()));

    const auto PWORKSPACE      = m_pWindow->m_workspace;
    const auto WORKSPACEOFFSET = PWORKSPACE && !m_pWindow->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D();

    return box.translate(WORKSPACEOFFSET);
}

PHLWINDOW CHyprBar::getOwner() {
    return m_pWindow.lock();
}

void CHyprBar::updateRules() {
    const auto PWINDOW = m_pWindow.lock();
    bool prevHidden = m_hidden;
    auto prevForcedTitleColor = m_bForcedTitleColor;
    // NOTE: The following loop assumes you have a rules vector. If not, comment it out or implement as needed.
    // for (auto& r : rules) { ... }
    auto rules = PWINDOW->m_matchedRules;

    m_bForcedBarHeight  = std::nullopt;
    m_bForcedBarPadding = std::nullopt;
    m_bForcedBarColor   = std::nullopt;
    m_bForcedBarBlur = std::nullopt;
    m_bForcedBarPartOfWindow = std::nullopt;
    m_bForcedBarPrecedenceOverBorder = std::nullopt;
    m_bForcedOnDoubleClick = std::nullopt;

    m_bForcedBarTitleEnabled = std::nullopt;
    m_bForcedBarTextFont = std::nullopt;
    m_bForcedBarTextSize = std::nullopt;
    m_bForcedBarTextAlign = std::nullopt;
    m_bForcedTitleColor = std::nullopt;
    m_bForcedBarCustomTitle = std::nullopt;
    
    m_bForcedIconOnHover = std::nullopt;
    m_bForcedBarButtonsAlignment = std::nullopt;
    m_bForcedBarButtonPadding = std::nullopt;
    m_bForcedInactiveButtonColor = std::nullopt;
    m_hidden           = false;

    m_windowRuleButtons.clear();

    for (auto& r : rules) {
        applyRule(r);
    }

    if (prevHidden != m_hidden)
        g_pDecorationPositioner->repositionDeco(this);
    if (prevForcedTitleColor.has_value() != m_bForcedTitleColor.has_value() ||
        (prevForcedTitleColor.has_value() && m_bForcedTitleColor.has_value() &&
         (prevForcedTitleColor.value().r != m_bForcedTitleColor.value().r ||
          prevForcedTitleColor.value().g != m_bForcedTitleColor.value().g ||
          prevForcedTitleColor.value().b != m_bForcedTitleColor.value().b ||
          prevForcedTitleColor.value().a != m_bForcedTitleColor.value().a)))
        m_bTitleColorChanged = true;
}

void CHyprBar::applyRule(const SP<CWindowRule>& r) {
    // Extract argument after first space, or empty string if not found
    std::string arg;
    size_t spacePos = r->m_rule.find_first_of(' ');
    if (spacePos != std::string::npos)
        arg = r->m_rule.substr(spacePos + 1);
    else
        arg = "";

    // Bar Window Rules
    if (r->m_rule == "plugin:hyprbars:nobar")
        m_hidden = true;
    else if (r->m_rule.find("plugin:hyprbars:bar_height") == 0) {
    auto res = configStringToIntOpt(arg);
        m_bForcedBarHeight = res.has_value() ? static_cast<int>(res.value()) : 0;
    } else if (r->m_rule.find("plugin:hyprbars:bar_padding") == 0) {
    auto res = configStringToIntOpt(arg);
        m_bForcedBarPadding = res.has_value() ? static_cast<int>(res.value()) : 0;
    } else if (r->m_rule.find("plugin:hyprbars:bar_color") == 0) {
    auto res = configStringToIntOpt(arg);
        m_bForcedBarColor = CHyprColor(res.has_value() ? static_cast<int>(res.value()) : 0);
    } else if (r->m_rule.find("plugin:hyprbars:bar_blur") == 0) {
    auto res = configStringToIntOpt(arg);
        m_bForcedBarBlur = res.has_value() ? static_cast<int>(res.value()) : 0;
    } else if (r->m_rule.find("plugin:hyprbars:bar_part_of_window") == 0) {
    auto res = configStringToIntOpt(arg);
        m_bForcedBarPartOfWindow = res.has_value() ? static_cast<int>(res.value()) : 0;
    } else if (r->m_rule.find("plugin:hyprbars:bar_precedence_over_border") == 0) {
    auto res = configStringToIntOpt(arg);
        m_bForcedBarPrecedenceOverBorder = res.has_value() ? static_cast<int>(res.value()) : 0;
    } else if (r->m_rule.find("plugin:hyprbars:on_double_click") == 0)
        m_bForcedOnDoubleClick = arg;
    // Title Window Rules
    else if (r->m_rule.find("plugin:hyprbars:bar_title_enabled") == 0) {
    auto res = configStringToIntOpt(arg);
        m_bForcedBarTitleEnabled = res.has_value() ? static_cast<int>(res.value()) : 0;
    } else if (r->m_rule.find("plugin:hyprbars:bar_text_font") == 0)
        m_bForcedBarTextFont = arg;
    else if (r->m_rule.find("plugin:hyprbars:bar_text_size") == 0) {
    auto res = configStringToIntOpt(arg);
        m_bForcedBarTextSize = res.has_value() ? static_cast<int>(res.value()) : 0;
    } else if (r->m_rule.find("plugin:hyprbars:bar_text_align") == 0)
        m_bForcedBarTextAlign = arg;
    else if (r->m_rule.find("plugin:hyprbars:title_color") == 0) {
    auto res = configStringToIntOpt(arg);
        m_bForcedTitleColor = CHyprColor(res.has_value() ? static_cast<int>(res.value()) : 0);
    } else if (r->m_rule.find("plugin:hyprbars:hyprbars-title") == 0)
        m_bForcedBarCustomTitle = arg;
    // Buttons Window Rules
    else if (r->m_rule.find("plugin:hyprbars:icon_on_hover") == 0) {
        auto res = configStringToIntOpt(arg);
        m_bForcedIconOnHover = res.has_value() ? static_cast<int>(res.value()) : 0;
    } else if (r->m_rule.find("plugin:hyprbars:bar_buttons_alignment") == 0)
        m_bForcedBarButtonsAlignment = arg;
    else if (r->m_rule.find("plugin:hyprbars:bar_button_padding") == 0) {
    auto res = configStringToIntOpt(arg);
        m_bForcedBarButtonPadding = res.has_value() ? static_cast<int>(res.value()) : 0;
    } else if (r->m_rule.find("plugin:hyprbars:inactive_button_color") == 0) {
    auto res = configStringToIntOpt(arg);
        m_bForcedInactiveButtonColor = CHyprColor(res.has_value() ? static_cast<int>(res.value()) : 0);
    } else if (r->m_rule.find("plugin:hyprbars:hyprbars-button") == 0) {
        auto params = splitByDelimiter(arg, ">|<");
        if (params.size() >= 4) {
            WindowRuleButton btn;
            auto res0 = configStringToIntOpt(params[0]);
            btn.bgcol = CHyprColor(res0.has_value() ? static_cast<int>(res0.value()) : 0);
            btn.size = std::stoi(params[1]);
            btn.icon = params[2];
            btn.cmd = params[3];
            if (params.size() >= 5) {
                auto res4 = configStringToIntOpt(params[4]);
                btn.fgcol = CHyprColor(res4.has_value() ? static_cast<int>(res4.value()) : 0);
                btn.userfg = true;
            }
            btn.iconTex = makeShared<CTexture>();
            m_windowRuleButtons.push_back(btn);
        }
    }
}

void CHyprBar::damageOnButtonHover() {
    static auto* const PBARPADDING       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_padding")->getDataStaticPtr();
    static auto* const PBARBUTTONPADDING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_button_padding")->getDataStaticPtr();
    static auto* const PHEIGHT           = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->getDataStaticPtr();
    static auto* const PALIGNBUTTONS     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();
    
    const std::string& buttonsAlign = m_bForcedBarButtonsAlignment.value_or(*PALIGNBUTTONS);
    const bool BUTTONSRIGHT = buttonsAlign != "left";

    float              offset = m_bForcedBarPadding.value_or(**PBARPADDING);

    const auto         COORDS = cursorRelativeToBar();

    if (!m_windowRuleButtons.empty()) {
        for (auto& b : m_windowRuleButtons) {
            const auto BARBUF     = Vector2D{(int)assignedBoxGlobal().w, m_bForcedBarHeight.value_or(**PHEIGHT)};
            Vector2D   currentPos = Vector2D{(BUTTONSRIGHT ? BARBUF.x - m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) - b.size - offset : offset), (BARBUF.y - b.size) / 2.0}.floor();

            bool       hover = VECINRECT(COORDS, currentPos.x, currentPos.y, currentPos.x + b.size + m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING), currentPos.y + b.size);

            if (hover != m_bButtonHovered) {
                m_bButtonHovered = hover;
                damageEntire();
            }

            offset += m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) + b.size;
        }
    } else {
        for (auto& b : g_pGlobalState->buttons) {
            const auto BARBUF     = Vector2D{(int)assignedBoxGlobal().w, m_bForcedBarHeight.value_or(**PHEIGHT)};
            Vector2D   currentPos = Vector2D{(BUTTONSRIGHT ? BARBUF.x - m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) - b.size - offset : offset), (BARBUF.y - b.size) / 2.0}.floor();

            bool       hover = VECINRECT(COORDS, currentPos.x, currentPos.y, currentPos.x + b.size + m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING), currentPos.y + b.size);

            if (hover != m_bButtonHovered) {
                m_bButtonHovered = hover;
                damageEntire();
            }

            offset += m_bForcedBarButtonPadding.value_or(**PBARBUTTONPADDING) + b.size;
        }

    }
}