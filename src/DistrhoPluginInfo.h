#pragma once

#define DISTRHO_PLUGIN_NAME    "Classic Reverb RE-04"
#define DISTRHO_PLUGIN_URI     "https://github.com/AnClark/ClassicReverb-RE04"
#define DISTRHO_PLUGIN_BRAND   "AnClark Liu"
#define DISTRHO_PLUGIN_CLAP_ID "studio.anclark.classic.reverb.re04"

#define DISTRHO_PLUGIN_NUM_INPUTS   2
#define DISTRHO_PLUGIN_NUM_OUTPUTS  2
#define DISTRHO_PLUGIN_IS_RT_SAFE   1
#define DISTRHO_PLUGIN_WANT_TIMEPOS 0

#define DISTRHO_PLUGIN_HAS_UI          1
#define DISTRHO_UI_USE_CUSTOM           1
#define DISTRHO_UI_CUSTOM_INCLUDE_PATH  "DearImGui.hpp"
#define DISTRHO_UI_CUSTOM_WIDGET_TYPE   DGL_NAMESPACE::ImGuiTopLevelWidget
#define DISTRHO_UI_DEFAULT_WIDTH         750 + 120 - 6  // Base width + right panel width
#define DISTRHO_UI_DEFAULT_HEIGHT        120

#define DISTRHO_PLUGIN_IS_SYNTH        0

//#define DISTRHO_PLUGIN_WANT_PROGRAMS   1
//#define DISTRHO_PLUGIN_NUM_PROGRAMS    33

/* VST2 unique ID – must be a bare 4-character token (no quotes, no commas) */
#define DISTRHO_PLUGIN_UNIQUE_ID       CRV4
