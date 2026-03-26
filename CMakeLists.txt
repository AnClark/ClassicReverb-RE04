cmake_minimum_required(VERSION 3.15)
project(ClassicReverb VERSION 1.0.0)

# ─────────────────────────────────────────────────────────────────────────────
# DPF (DISTRHO Plugin Framework)
# ─────────────────────────────────────────────────────────────────────────────
set(DPF_ROOT "${PROJECT_SOURCE_DIR}/dpf" CACHE PATH "Path to the DPF source tree")

if(NOT EXISTS "${DPF_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "DPF not found at '${DPF_ROOT}'.\n"
        "Please pass -DDPF_ROOT=<path> to CMake or place the dpf folder next to ClassicReverb.")
endif()

add_subdirectory("${DPF_ROOT}" dpf EXCLUDE_FROM_ALL)

# ─────────────────────────────────────────────────────────────────────────────
# Options
# ─────────────────────────────────────────────────────────────────────────────

option (CONFIG_BUILD_JACK_STANDALONE "Build JACK standalone format (for debugging)")
if (CONFIG_BUILD_JACK_STANDALONE)
    set (TARGET_JACK jack)
endif ()

# ─────────────────────────────────────────────────────────────────────────────
# Plugin targets
# ─────────────────────────────────────────────────────────────────────────────
dpf_add_plugin(ClassicReverb
    TARGETS     vst2 vst3 clap ${TARGET_JACK}
    FILES_DSP
        src/ClassicReverbPlugin.cpp
        src/CoreDSP.cpp
    FILES_UI
        src/UI.cpp
        src/UISubroutines.cpp
        src/PresetManager.cpp
        src/subwindows/PresetManagerWindow.cpp
        src/subwindows/FileBrowserStuff.cpp
        src/subwindows/MessageBoxStuff.cpp
        widgets/dpf-imgui/DearImGui.cpp
        widgets/imgui-knobs-mod/imgui-knobs.cpp
    NO_SHARED_RESOURCES
)

# DistrhoPluginInfo.h must be findable by the DPF internal headers
# DPF adds <dpf>/distrho to the include path, but not the project source dir.
target_include_directories(ClassicReverb PUBLIC
    src
    widgets/dpf-widgets
    widgets/dpf-imgui
    widgets/dpf-imgui/DearImGui
    widgets/imgui-knobs-mod
    widgets/imgui-ext
    ${PROJECT_BINARY_DIR}
    3rdparty
)

# Turn on features on external dependencies (e.g. Widgets)
target_compile_definitions (ClassicReverb-ui PRIVATE IMGUI_KNOBS_SET_CURSOR=1)  # Enable cursor changes in knobs (AnClark's Mod).

# Ensure C++17 (needed for std::clamp etc.)
target_compile_features(ClassicReverb PUBLIC cxx_std_17)

# Add configure file
configure_file (src/config.h.in ${PROJECT_BINARY_DIR}/config.h @ONLY)

# ─────────────────────────────────────────────────────────────────────────────
# Compiler warnings / options
# ─────────────────────────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(ClassicReverb PRIVATE /W3 /WX-)
else()
    target_compile_options(ClassicReverb PRIVATE
        -Wall -Wno-unused-variable -ffast-math)
endif()
