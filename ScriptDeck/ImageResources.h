#pragma once
#include "ResourceIds.h"
#include <array>
struct ImageResourceDefinition { const char* id; const char* name; unsigned short nativeId; };
inline constexpr std::array<ImageResourceDefinition, 1> ImageResources = {{
    {"scriptdeck.logo", "ScriptDeckロゴ", IDR_SCRIPTDECK_LOGO}
}};
