// The only translation unit compiling raygui, plus its embeddable styles.
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include <raygui/styles/style_advance.h>
#include <raygui/styles/style_amber.h>
#include <raygui/styles/style_ashes.h>
#include <raygui/styles/style_bluish.h>
#include <raygui/styles/style_brick.h>
#include <raygui/styles/style_candy.h>
#include <raygui/styles/style_cherry.h>
#include <raygui/styles/style_cyber.h>
#include <raygui/styles/style_dark.h>
#include <raygui/styles/style_enefete.h>
#include <raygui/styles/style_genesis.h>
#include <raygui/styles/style_jungle.h>
#include <raygui/styles/style_lavanda.h>
#include <raygui/styles/style_pocket.h>
#include <raygui/styles/style_rltech.h>
#include <raygui/styles/style_sunny.h>
#include <raygui/styles/style_terminal.h>
#include <raygui/styles/style_turbo.h>
#include <raygui/styles/style_wisteria.h>

#include "internal.h"
#include <array>
#include <utility>

namespace gui::detail {

bool load_builtin_style(std::string_view name) {
    static constexpr std::array<std::pair<std::string_view, void (*)()>, 19> styles{ {
        { "advance", GuiLoadStyleAdvance },   { "amber", GuiLoadStyleAmber },
        { "ashes", GuiLoadStyleAshes },       { "bluish", GuiLoadStyleBluish },
        { "brick", GuiLoadStyleBrick },       { "candy", GuiLoadStyleCandy },
        { "cherry", GuiLoadStyleCherry },     { "cyber", GuiLoadStyleCyber },
        { "dark", GuiLoadStyleDark },         { "enefete", GuiLoadStyleEnefete },
        { "genesis", GuiLoadStyleGenesis },   { "jungle", GuiLoadStyleJungle },
        { "lavanda", GuiLoadStyleLavanda },   { "pocket", GuiLoadStylePocket },
        { "rltech", GuiLoadStyleRLTech },     { "sunny", GuiLoadStyleSunny },
        { "terminal", GuiLoadStyleTerminal }, { "turbo", GuiLoadStyleTurbo },
        { "wisteria", GuiLoadStyleWisteria },
    } };
    for (const auto &[style, load] : styles)
        if (style == name) {
            load();
            return true;
        }
    return false;
}

} // namespace gui::detail
