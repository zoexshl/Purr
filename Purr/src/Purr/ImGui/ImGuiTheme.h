#pragma once

#include "Purr/Core.h"

namespace Purr {

    enum class ImGuiThemeKind
    {
        DarkPink,
        DarkClassic
    };

    PURR_API void ApplyImGuiTheme(ImGuiThemeKind theme);

}
