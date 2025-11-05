#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <any>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include <algorithm>
#include <ranges>

#include "barDeco.hpp"
#include "globals.hpp"
#include "hyprbars_utils.hpp"
// persistent file logger for post-crash inspection
#include "hyprbars_logger.hpp"

// Custom Title Variables
#include <cstdio>
#include <sstream>

Hyprlang::CParseResult onNewTitleVar(const char* K, const char* V) {
    std::string v = V;
    auto comma = v.find(',');
    Hyprlang::CParseResult result;
    if (comma == std::string::npos) {
        result.setError("title-var must be in the form name,command");
        return result;
    }
    std::string name = v.substr(0, comma);
    std::string cmd = v.substr(comma + 1);
    // trim spaces
    name.erase(0, name.find_first_not_of(" \t"));
    name.erase(name.find_last_not_of(" \t") + 1);
    cmd.erase(0, cmd.find_first_not_of(" \t"));
    cmd.erase(cmd.find_last_not_of(" \t") + 1);

    // Run the command and capture output
    std::string output;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            output += buffer;
        }
        pclose(pipe);
        // Remove trailing newline
        if (!output.empty() && output.back() == '\n')
            output.pop_back();
    }
    g_titleVars[name] = output;
    return result;
}


// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static void onNewWindow(void* self, std::any data) {
    // data is guaranteed
    const auto PWINDOW = std::any_cast<PHLWINDOW>(data);
    // trace entry with title and mapped flag (best-effort)
    {
        char buf[512];
        snprintf(buf, sizeof(buf), "onNewWindow: title='%s' mapped=%d", PWINDOW->m_title.c_str(), PWINDOW->m_isMapped ? 1 : 0);
        hyprbars::lowlevel_log(buf);
    }
    try {
        if (!PWINDOW->m_X11DoesntWantBorders) {
            if (std::ranges::any_of(PWINDOW->m_windowDecorations, [](const auto& d) { return d->getDisplayName() == "Hyprbar"; }))
                return;

            auto bar = makeUnique<CHyprBar>(PWINDOW);
            g_pGlobalState->bars.emplace_back(bar);
            bar->m_self = bar;
            HyprlandAPI::addWindowDecoration(PHANDLE, PWINDOW, std::move(bar));
            hyprbars::lowlevel_log("onNewWindow: addWindowDecoration done");
        }
    } catch (const std::exception& e) {
    HyprlandAPI::addNotification(PHANDLE, std::string("[hyprbars] onNewWindow exception: ") + e.what(), CHyprColor{1.0, 0.2, 0.2, 1.0}, 8000);
    hyprbars::logException("onNewWindow", e);
    } catch (...) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprbars] onNewWindow unknown exception", CHyprColor{1.0, 0.2, 0.2, 1.0}, 8000);
        hyprbars::logUnknown("onNewWindow");
    }
}

static void onCloseWindow(void* self, std::any data) {
    // data is guaranteed
    const auto PWINDOW = std::any_cast<PHLWINDOW>(data);
    try {
        const auto BARIT = std::find_if(g_pGlobalState->bars.begin(), g_pGlobalState->bars.end(), [PWINDOW](const auto& bar) { return bar->getOwner() == PWINDOW; });

        if (BARIT == g_pGlobalState->bars.end())
            return;

        // we could use the API but this is faster + it doesn't matter here that much.
        PWINDOW->removeWindowDeco(BARIT->get());
    } catch (const std::exception& e) {
    HyprlandAPI::addNotification(PHANDLE, std::string("[hyprbars] onCloseWindow exception: ") + e.what(), CHyprColor{1.0, 0.2, 0.2, 1.0}, 8000);
    hyprbars::logException("onCloseWindow", e);
    } catch (...) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprbars] onCloseWindow unknown exception", CHyprColor{1.0, 0.2, 0.2, 1.0}, 8000);
        hyprbars::logUnknown("onCloseWindow");
    }
}

static void onPreConfigReload() {
    try {
        g_pGlobalState->buttons.clear();
    } catch (const std::exception& e) {
    HyprlandAPI::addNotification(PHANDLE, std::string("[hyprbars] onPreConfigReload exception: ") + e.what(), CHyprColor{1.0, 0.2, 0.2, 1.0}, 8000);
    hyprbars::logException("onPreConfigReload", e);
    } catch (...) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprbars] onPreConfigReload unknown exception", CHyprColor{1.0, 0.2, 0.2, 1.0}, 8000);
        hyprbars::logUnknown("onPreConfigReload");
    }
}

static void onUpdateWindowRules(PHLWINDOW window) {
    try {
        const auto BARIT = std::find_if(g_pGlobalState->bars.begin(), g_pGlobalState->bars.end(), [window](const auto& bar) { return bar->getOwner() == window; });

        if (BARIT == g_pGlobalState->bars.end())
            return;

        (*BARIT)->updateRules();
        window->updateWindowDecos();
    } catch (const std::exception& e) {
    HyprlandAPI::addNotification(PHANDLE, std::string("[hyprbars] onUpdateWindowRules exception: ") + e.what(), CHyprColor{1.0, 0.2, 0.2, 1.0}, 8000);
    hyprbars::logException("onUpdateWindowRules", e);
    } catch (...) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprbars] onUpdateWindowRules unknown exception", CHyprColor{1.0, 0.2, 0.2, 1.0}, 8000);
        hyprbars::logUnknown("onUpdateWindowRules");
    }
}

Hyprlang::CParseResult onNewButton(const char* K, const char* V) {
    std::string            v = V;
    CVarList               vars(v);

    Hyprlang::CParseResult result;

    // hyprbars-button = bgcolor, size, icon, action, fgcolor

    if (vars[0].empty() || vars[1].empty()) {
        result.setError("bgcolor and size cannot be empty");
        return result;
    }

    float size = 10;
    try {
        size = std::stof(vars[1]);
    } catch (std::exception& e) {
        result.setError("failed to parse size");
        return result;
    }

    bool userfg  = false;
    auto fgcolor = parseConfigInt("rgb(ffffff)");
    auto bgcolor = parseConfigInt(vars[0]);

    if (!bgcolor) {
        result.setError("invalid bgcolor");
        return result;
    }

    if (vars.size() == 5) {
    userfg  = true;
    fgcolor = parseConfigInt(vars[4]);
    }

    if (!fgcolor) {
        result.setError("invalid fgcolor");
        return result;
    }

    g_pGlobalState->buttons.push_back(SHyprButton{vars[3], userfg, *fgcolor, *bgcolor, size, vars[2]});

    for (auto& b : g_pGlobalState->bars) {
        b->m_bButtonsDirty = true;
    }

    return result;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;
    // very early low-level trace for crash investigation
    hyprbars::lowlevel_log("PLUGIN_INIT: enter");

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprbars] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        hyprbars::log("PLUGIN_INIT: version mismatch between headers and running hyprland (" + HASH + " != " + CLIENT_HASH + ")");
        throw std::runtime_error("[hb] Version mismatch");
    }

    hyprbars::lowlevel_log("PLUGIN_INIT: version check OK");

    g_pGlobalState = makeUnique<SGlobalState>();

    hyprbars::lowlevel_log("PLUGIN_INIT: allocated global state");

    static auto P = HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow",
                                                       [&](void* self, SCallbackInfo& info, std::any data) {
                                                           try {
                                                               onNewWindow(self, data);
                                                           } catch (const std::exception& e) {
                                                   HyprlandAPI::addNotification(PHANDLE, std::string("[hyprbars] openWindow handler exception: ") + e.what(), CHyprColor{1.0, 0.2, 0.2, 1.0}, 8000);
                                                   hyprbars::logException("openWindow handler", e);
                                                           } catch (...) {
                                                   HyprlandAPI::addNotification(PHANDLE, "[hyprbars] openWindow handler unknown exception", CHyprColor{1.0, 0.2, 0.2, 1.0}, 8000);
                                                   hyprbars::logUnknown("openWindow handler");
                                                           }
                                                       });
    // static auto P2 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "closeWindow", [&](void* self, SCallbackInfo& info, std::any data) { onCloseWindow(self, data); });
    static auto P3 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "windowUpdateRules",
                                                          [&](void* self, SCallbackInfo& info, std::any data) {
                                                              try {
                                                                  onUpdateWindowRules(std::any_cast<PHLWINDOW>(data));
                                                              } catch (const std::exception& e) {
                                                     HyprlandAPI::addNotification(PHANDLE, std::string("[hyprbars] windowUpdateRules handler exception: ") + e.what(), CHyprColor{1.0, 0.2, 0.2, 1.0}, 8000);
                                                     hyprbars::logException("windowUpdateRules handler", e);
                                                              } catch (...) {
                                                     HyprlandAPI::addNotification(PHANDLE, "[hyprbars] windowUpdateRules handler unknown exception", CHyprColor{1.0, 0.2, 0.2, 1.0}, 8000);
                                                     hyprbars::logUnknown("windowUpdateRules handler");
                                                              }
                                                          });

    hyprbars::lowlevel_log("PLUGIN_INIT: callbacks registered");

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_color", Hyprlang::INT{*parseConfigInt("rgba(33333388)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_height", Hyprlang::INT{15});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:col.text", Hyprlang::INT{*parseConfigInt("rgba(ffffffff)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_text_size", Hyprlang::INT{10});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_title_enabled", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_blur", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_text_font", Hyprlang::STRING{"Sans"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_text_align", Hyprlang::STRING{"center"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_part_of_window", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_precedence_over_border", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment", Hyprlang::STRING{"right"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_padding", Hyprlang::INT{7});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_button_padding", Hyprlang::INT{5});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:enabled", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:icon_on_hover", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:inactive_button_color", Hyprlang::INT{0}); // unset
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:on_double_click", Hyprlang::STRING{""});

    HyprlandAPI::addConfigKeyword(PHANDLE, "title-var", onNewTitleVar, Hyprlang::SHandlerOptions{});

    HyprlandAPI::addConfigKeyword(PHANDLE, "hyprbars-button", onNewButton, Hyprlang::SHandlerOptions{});
    static auto P4 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "preConfigReload", [&](void* self, SCallbackInfo& info, std::any data) { onPreConfigReload(); });

    hyprbars::lowlevel_log("PLUGIN_INIT: config keywords and preConfigReload registered");

    // add deco to existing windows
    for (auto& w : g_pCompositor->m_windows) {
        if (w->isHidden() || !w->m_isMapped)
            continue;

        onNewWindow(nullptr /* unused */, std::any(w));
    }

    HyprlandAPI::reloadConfig();

    hyprbars::lowlevel_log("PLUGIN_INIT: reloadConfig called");

    HyprlandAPI::addNotification(PHANDLE, "[hyprbars] Initialized successfully!", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"hyprbars", "A plugin to add title bars to windows.", "AragornDude and Vaxry", "2.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    for (auto& m : g_pCompositor->m_monitors)
        m->m_scheduledRecalc = true;

    g_pHyprRenderer->m_renderPass.removeAllOfType("CBarPassElement");
}
