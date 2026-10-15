cmake_minimum_required(VERSION 3.20)
project(mcov LANGUAGES C CXX VERSION 0.1.0)
enable_language(RC)
configure_file(src/common/menu_fonts.rc.in "${CMAKE_CURRENT_BINARY_DIR}/menu_fonts.rc" @ONLY)

# ---------------------------------------------------------------------------
# Two targets, deliberately separated:
#
#   mcov_overlay (exe) - runs as its own process. D3D11 + ImGui + Win32. Knows
#                        nothing about JNI.
#   mcov_agent   (dll) - runs INSIDE javaw.exe. JNI + JVMTI. Never links ImGui or a renderer.
#
# The split is not stylistic: JNI_GetCreatedJavaVMs and JavaVM* only mean
# anything inside the JVM's own address space, so the JNI code physically cannot
# live in the overlay process. See src/agent/jvm_interface.hpp.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    # Portable EXEs und DLL: keine separat zu installierenden VC-Runtime-Dateien.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Der Combat-Kern hat keine JNI-/Win32-Abhängigkeiten. Der konkrete Demo-Transport
# simuliert Spiel-Cooldown, Raycast und Silent-Kamera; keine Live-Netzwerkbindung.
add_library(mcov_combat INTERFACE)
target_include_directories(mcov_combat INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(mcov_combat INTERFACE cxx_std_20)

if(NOT WIN32)
    message(FATAL_ERROR "mcov targets Windows only (layered windows, Win32 process APIs).")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR
        "A 64-bit build is required. Minecraft 1.21 runs on a 64-bit JVM, and JNI "
        "has no cross-bitness path - a 32-bit agent simply cannot be loaded.")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Build JDK 17+; attach runtime must match the game's Java major (17/21/25).
# ---------------------------------------------------------------------------
# MCOV_JAVA_HOME lets you pin the JDK explicitly; otherwise we fall back to the
# JAVA_HOME environment variable and then to CMake's own registry probing.
set(MCOV_JAVA_HOME "" CACHE PATH "JDK 17+ root (contains include/jni.h and include/win32)")

if(MCOV_JAVA_HOME)
    set(ENV{JAVA_HOME} "${MCOV_JAVA_HOME}")
endif()

find_package(JNI REQUIRED)

# FindJNI gives us the header dirs; we never link jvm.lib. The agent gets its
# JNIEnv* from Agent_OnAttach, and the JNI_GetCreatedJavaVMs fallback path
# resolves through GetProcAddress at runtime. Linking jvm.lib would add a hard
# load-time dependency on a specific JDK for no benefit.
if(NOT JNI_INCLUDE_DIRS)
    message(FATAL_ERROR "JNI headers not found. Set -DMCOV_JAVA_HOME=<jdk17+ root>.")
endif()

# jvmti.h ships beside jni.h; verify it so a JRE-only path fails loudly here
# rather than with a confusing include error later.
find_file(MCOV_JVMTI_HEADER jvmti.h PATHS ${JNI_INCLUDE_DIRS} NO_DEFAULT_PATH)
if(NOT MCOV_JVMTI_HEADER)
    message(FATAL_ERROR
        "jvmti.h not found next to jni.h. MCOV_JAVA_HOME must point at a full JDK, "
        "not a JRE.")
endif()

# Report (and sanity-check) the JDK version from the release manifest.
list(GET JNI_INCLUDE_DIRS 0 _mcov_jni_include)
get_filename_component(MCOV_JDK_ROOT "${_mcov_jni_include}" DIRECTORY)
if(EXISTS "${MCOV_JDK_ROOT}/release")
    file(STRINGS "${MCOV_JDK_ROOT}/release" _mcov_release_lines REGEX "^JAVA_VERSION=")
    if(_mcov_release_lines)
        string(REGEX REPLACE "JAVA_VERSION=\"?([0-9]+).*" "\\1" MCOV_JDK_MAJOR "${_mcov_release_lines}")
        message(STATUS "mcov: JDK ${MCOV_JDK_MAJOR} at ${MCOV_JDK_ROOT}")
        if(MCOV_JDK_MAJOR LESS 17)
            message(FATAL_ERROR "Build with a full JDK 17 or newer.")
        endif()
    endif()
endif()

# javac/jar build both the attach helper and the required frame synchronization JARs.
find_package(Java 17 COMPONENTS Development REQUIRED)

# ---------------------------------------------------------------------------
# Third-party: Dear ImGui
# ---------------------------------------------------------------------------
include(FetchContent)

# Keep the pinned ImGui API/font atlas version for this UI change.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.9b
    GIT_SHALLOW    TRUE)

# ImGui ships no CMakeLists, so MakeAvailable only populates the source tree.
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)

target_link_libraries(imgui PUBLIC d3d11 dxgi d3dcompiler)
target_compile_features(imgui PUBLIC cxx_std_20)

# Nur der MIT-lizenzierte Material-Renderer, nicht der ImGui-Fork/demo/Recorder.
add_library(mcov_liquid STATIC src/common/liquid_glass.cpp src/common/windows_backdrop_capture.cpp
    third_party/glass/glass.cpp third_party/glass/backdrop.cpp)
target_include_directories(mcov_liquid PUBLIC src/common PRIVATE third_party/glass)
target_link_libraries(mcov_liquid PUBLIC imgui user32 winmm windowsapp)
target_compile_definitions(mcov_liquid PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(mcov_liquid PRIVATE /utf-8 /W3)
endif()

# ---------------------------------------------------------------------------
# Shared compile settings
# ---------------------------------------------------------------------------
add_library(mcov_settings INTERFACE)

target_compile_definitions(mcov_settings INTERFACE
    WIN32_LEAN_AND_MEAN
    NOMINMAX                 # or windows.h's min/max macros break <algorithm>
    UNICODE
    _UNICODE)

if(MSVC)
    target_compile_options(mcov_settings INTERFACE
        /W4
        /permissive-
        /Zc:__cplusplus      # without this __cplusplus reports 199711L
        /utf-8
        /MP
        /wd4100)             # unreferenced formal parameter: noisy on JNI callbacks
else()
    # clang-cl / MinGW
    target_compile_options(mcov_settings INTERFACE -Wall -Wextra -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------------------
# mcov_overlay - the external process
# ---------------------------------------------------------------------------
add_library(mcov_auth STATIC src/common/keyauth.cpp src/common/auth_handoff.cpp
    third_party/auth/monocypher/monocypher.c third_party/auth/monocypher/optional/monocypher-ed25519.c)
target_include_directories(mcov_auth PUBLIC src PRIVATE third_party/auth third_party/auth/monocypher)
target_link_libraries(mcov_auth PUBLIC mcov_settings winhttp bcrypt advapi32)

add_executable(mcov_overlay
    "${CMAKE_CURRENT_BINARY_DIR}/menu_fonts.rc"
    src/main.cpp
    src/common/flat_style.cpp
    src/overlay/overlay.cpp
    src/overlay/overlay_input.cpp
    src/overlay/overlay_configs.cpp
    src/overlay/overlay_hud.cpp
    src/overlay/overlay_world_visuals.cpp
    src/overlay/overlay_rockstar.cpp
    src/overlay/overlay_utilities.cpp
    src/overlay/overlay_smp.cpp
    src/overlay/smp_features.cpp
    src/overlay/esp_renderer.cpp
    src/overlay/visual_draw.cpp
    src/overlay/item_icon_atlas_dx11.cpp
    src/overlay/control_panel.cpp
    src/overlay/overlay_modules.cpp
    src/overlay/combat_menu.cpp
    src/overlay/aura_renderer.cpp
    src/overlay/friend_hat_renderer.cpp
    src/overlay/target_cube_renderer.cpp
    src/overlay/world_visuals.cpp
    src/overlay/rockstar_features.cpp
    src/overlay/aura_blend_dx11.cpp
    src/overlay/depth_renderer.cpp
    src/overlay/process.cpp
    src/common/ipc.cpp
    src/common/log.cpp)

target_include_directories(mcov_overlay PRIVATE src third_party/glass) # Embedded Inter fonts only

# Bake in the JDK we built against. Minecraft launchers routinely ship a JRE,
# which has no jdk.attach module and therefore cannot load the agent; this gives
# the overlay a known-good fallback without requiring JAVA_HOME to be set.
if(MCOV_JDK_ROOT)
    target_compile_definitions(mcov_overlay PRIVATE MCOV_BUILD_JDK_HOME="${MCOV_JDK_ROOT}")
endif()

target_link_libraries(mcov_overlay PRIVATE
    mcov_auth
    mcov_liquid
    winmm
    mcov_settings
    imgui
    d3d11
    dxgi
    d3dcompiler
    user32
    gdi32
    shell32
    dwmapi ole32 windowscodecs)   # DWM composition and the embedded PNG atlas

set_target_properties(mcov_overlay PROPERTIES OUTPUT_NAME "mcov_overlay")

# ---------------------------------------------------------------------------
# mcov_injector - standalone flat GUI for attaching the agent
#
# Independent of mcov_overlay: it reuses process.cpp for JVM discovery and the
# attach helper, and common/flat_app.cpp for the window + ImGui frame loop.
# ---------------------------------------------------------------------------
add_executable(mcov_injector WIN32
    "${CMAKE_CURRENT_BINARY_DIR}/menu_fonts.rc"
    src/injector/injector_main.cpp
    src/injector/launcher_ui.cpp
    src/injector/portable_package.cpp
    src/overlay/control_panel.cpp
    src/common/flat_style.cpp
    src/common/flat_app.cpp
    src/overlay/process.cpp
    src/common/ipc.cpp
    src/common/log.cpp)

target_include_directories(mcov_injector PRIVATE src src/common third_party/glass)

if(MCOV_JDK_ROOT)
    target_compile_definitions(mcov_injector PRIVATE MCOV_BUILD_JDK_HOME="${MCOV_JDK_ROOT}")
endif()

target_link_libraries(mcov_injector PRIVATE
    mcov_auth
    mcov_liquid
    mcov_settings
    imgui
    d3d11
    dxgi
    d3dcompiler
    user32
    gdi32
    shell32
    dwmapi ole32 uuid bcrypt)

# ---------------------------------------------------------------------------
# mcov_agent - the in-JVM library
# ---------------------------------------------------------------------------
add_library(mcov_agent SHARED
    src/agent/agent_main.cpp
    src/agent/base_texture_gl.cpp
    src/agent/jvm_interface.cpp
    src/common/ipc.cpp
    src/common/log.cpp)

target_include_directories(mcov_agent PRIVATE src ${JNI_INCLUDE_DIRS})
target_link_libraries(mcov_agent PRIVATE mcov_settings opengl32)

set_target_properties(mcov_agent PROPERTIES
    OUTPUT_NAME "mcov_agent"
    PREFIX ""            # no "lib" prefix: the loader asks for mcov_agent.dll
    CXX_VISIBILITY_PRESET default)

# Statically link the CRT into the agent. It gets loaded into a JVM that has no
# idea about our runtime, and a missing vcruntime140.dll surfaces as an opaque
# AgentLoadException with no further detail.
if(MSVC)
    set_property(TARGET mcov_agent PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---------------------------------------------------------------------------
# AgentLoader.java -> bin/tools/AgentLoader.class
# ---------------------------------------------------------------------------
if(Java_JAVAC_EXECUTABLE)
    set(MCOV_TOOLS_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tools)

    add_custom_command(
        OUTPUT ${MCOV_TOOLS_DIR}/AgentLoader.class
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MCOV_TOOLS_DIR}
        # Java 17 bytecode runs on all supported attach JDKs (17, 21, 25).
        COMMAND ${Java_JAVAC_EXECUTABLE} --release 17
                -d ${MCOV_TOOLS_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/AgentLoader.java
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/AgentLoader.java
        COMMENT "Compiling AgentLoader.java (JVM attach helper)"
        VERBATIM)

    add_custom_target(mcov_agent_loader ALL
        DEPENDS ${MCOV_TOOLS_DIR}/AgentLoader.class)
    set(MCOV_FRAME_CLASSES ${CMAKE_BINARY_DIR}/frame-sync-classes)
    set(MCOV_SIGNAL_CLASSES ${CMAKE_BINARY_DIR}/frame-signal-classes)
    set(MCOV_FRAME_SOURCES
        tools/frame-sync/FrameSynchronizer.java tools/frame-sync/FrameTransformer.java tools/frame-sync/FrameProfiles.java tools/frame-sync/DepthProfiles.java)
    list(TRANSFORM MCOV_FRAME_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
    add_custom_command(
        OUTPUT ${MCOV_TOOLS_DIR}/frame-sync-agent.jar ${MCOV_TOOLS_DIR}/frame-signal.jar
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MCOV_FRAME_CLASSES} ${MCOV_SIGNAL_CLASSES} ${MCOV_TOOLS_DIR}
        COMMAND ${Java_JAVAC_EXECUTABLE} --release 17 -d ${MCOV_SIGNAL_CLASSES}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/frame-sync/FrameSignal.java
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/frame-sync/BaseTextureBridge.java
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/frame-sync/BaseTextureProfiles.java
        COMMAND ${Java_JAVAC_EXECUTABLE} -source 17 -target 17
            --add-exports java.base/jdk.internal.org.objectweb.asm=ALL-UNNAMED
            -d ${MCOV_FRAME_CLASSES} ${MCOV_FRAME_SOURCES}
        COMMAND ${Java_JAR_EXECUTABLE} -J-Djava.io.tmpdir=${CMAKE_BINARY_DIR} --create --file ${MCOV_TOOLS_DIR}/frame-signal.jar
            -C ${MCOV_SIGNAL_CLASSES} .
        COMMAND ${Java_JAR_EXECUTABLE} -J-Djava.io.tmpdir=${CMAKE_BINARY_DIR} --create --file ${MCOV_TOOLS_DIR}/frame-sync-agent.jar
            --manifest ${CMAKE_CURRENT_SOURCE_DIR}/tools/frame-sync/manifest.mf -C ${MCOV_FRAME_CLASSES} .
        DEPENDS ${MCOV_FRAME_SOURCES} tools/frame-sync/FrameSignal.java tools/frame-sync/manifest.mf
            tools/frame-sync/BaseTextureBridge.java tools/frame-sync/BaseTextureProfiles.java
        COMMENT "Compiling Java frame synchronization helpers (Java 17 bytecode)"
        VERBATIM)
    add_custom_target(mcov_frame_agent ALL DEPENDS ${MCOV_TOOLS_DIR}/frame-sync-agent.jar ${MCOV_TOOLS_DIR}/frame-signal.jar)
    add_dependencies(mcov_agent_loader mcov_frame_agent)
    add_dependencies(mcov_overlay mcov_agent_loader)
endif()

# Deliberately NOT add_dependencies(mcov_overlay mcov_agent).
#
# The agent DLL cannot be relinked while it is mapped into a running Minecraft,
# and coupling the two targets meant a locked DLL also blocked every overlay
# rebuild. They are independent binaries; build "all" to get both.

message(STATUS "mcov: overlay -> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mcov_overlay.exe")
message(STATUS "mcov: agent   -> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mcov_agent.dll")

# Ein distributierbares EXE; alle separaten Artefakte bleiben Entwickler-Ausgaben.
set(MCOV_PORTABLE_RC "${CMAKE_BINARY_DIR}/portable_payload.rc")
set(MCOV_PORTABLE_HEADER "${CMAKE_BINARY_DIR}/portable_manifest.hpp")
add_custom_command(OUTPUT "${MCOV_PORTABLE_RC}" "${MCOV_PORTABLE_HEADER}"
    COMMAND ${CMAKE_COMMAND}
        "-DOVERLAY=$<TARGET_FILE:mcov_overlay>" "-DAGENT=$<TARGET_FILE:mcov_agent>"
        "-DLOADER=${MCOV_TOOLS_DIR}/AgentLoader.class" "-DFRAME=${MCOV_TOOLS_DIR}/frame-sync-agent.jar"
        "-DSIGNAL=${MCOV_TOOLS_DIR}/frame-signal.jar"
        "-DOUTPUT_RC=${MCOV_PORTABLE_RC}" "-DOUTPUT_HEADER=${MCOV_PORTABLE_HEADER}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/package_client.cmake"
    DEPENDS mcov_overlay mcov_agent mcov_agent_loader mcov_frame_agent
        "$<TARGET_FILE:mcov_overlay>" "$<TARGET_FILE:mcov_agent>"
        "${MCOV_TOOLS_DIR}/AgentLoader.class" "${MCOV_TOOLS_DIR}/frame-sync-agent.jar"
        "${MCOV_TOOLS_DIR}/frame-signal.jar" tools/package_client.cmake
    COMMENT "Embedding the complete portable client" VERBATIM)
add_executable(mcov WIN32 "${MCOV_PORTABLE_RC}" "${MCOV_PORTABLE_HEADER}"
    "${CMAKE_CURRENT_BINARY_DIR}/menu_fonts.rc"
    src/injector/injector_main.cpp src/injector/launcher_ui.cpp src/injector/portable_package.cpp
    src/overlay/control_panel.cpp src/common/flat_app.cpp src/common/flat_style.cpp
    src/overlay/process.cpp src/common/ipc.cpp src/common/log.cpp)
target_include_directories(mcov PRIVATE src third_party/glass "${CMAKE_BINARY_DIR}")
target_compile_definitions(mcov PRIVATE MCOV_BUNDLED=1 MCOV_BUILD_JDK_HOME="${MCOV_JDK_ROOT}")
target_link_libraries(mcov PRIVATE mcov_auth mcov_liquid mcov_settings imgui user32 gdi32 shell32 dwmapi ole32 uuid bcrypt)
set_target_properties(mcov PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/client/$<0:>")
message(STATUS "mcov: open ${CMAKE_BINARY_DIR}/client/mcov.exe (single-file client)")

# Fertiger Ordner mit zwei Dateien und eine ZIP fuer die Weitergabe.
set(MCOV_CLIENT_ARCHIVE "${CMAKE_BINARY_DIR}/mcov-client.zip")
add_custom_command(OUTPUT "${MCOV_CLIENT_ARCHIVE}" "${CMAKE_BINARY_DIR}/client/START.txt"
    COMMAND ${CMAKE_COMMAND} "-DCLIENT_EXE=$<TARGET_FILE:mcov>"
        "-DGUIDE=${CMAKE_CURRENT_SOURCE_DIR}/docs/client-start.txt"
        "-DARCHIVE=${MCOV_CLIENT_ARCHIVE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/distribute_client.cmake"
    DEPENDS mcov "$<TARGET_FILE:mcov>" docs/client-start.txt tools/distribute_client.cmake
    COMMENT "Packaging mcov.exe and the optional start guide" VERBATIM)
add_custom_target(mcov_distribution ALL DEPENDS "${MCOV_CLIENT_ARCHIVE}" "${CMAKE_BINARY_DIR}/client/START.txt")

# Optional headless checks for UI ownership, clipping and input. No JVM needed.
option(MCOV_BUILD_UI_CHECKS "Build the headless flat UI checks" OFF)
if(MCOV_BUILD_UI_CHECKS)
    enable_testing()
    add_executable(mcov_auth_check tests/keyauth.cpp)
    target_include_directories(mcov_auth_check PRIVATE third_party/auth third_party/auth/monocypher)
    target_link_libraries(mcov_auth_check PRIVATE mcov_auth)
    add_test(NAME keyauth COMMAND mcov_auth_check)
    add_executable(mcov_liquid_check tests/liquid_glass_rendering.cpp)
    target_include_directories(mcov_liquid_check PRIVATE src third_party/glass)
    target_link_libraries(mcov_liquid_check PRIVATE mcov_settings mcov_liquid)
    add_test(NAME liquid_glass COMMAND mcov_liquid_check)
    # Nur explizit --live starten: die regulaere Testsuite liest keinen Desktop.
    add_executable(mcov_backdrop_capture_check tests/backdrop_capture_live.cpp)
    target_include_directories(mcov_backdrop_capture_check PRIVATE src third_party/glass)
    target_link_libraries(mcov_backdrop_capture_check PRIVATE mcov_settings mcov_liquid)
    if(MSVC)
        target_link_options(mcov_backdrop_capture_check PRIVATE /MAP)
    endif()
    set_tests_properties(liquid_glass PROPERTIES TIMEOUT 30)
    add_executable(mcov_launcher_window_check
        "${CMAKE_CURRENT_BINARY_DIR}/menu_fonts.rc"
        tests/launcher_window_rendering.cpp src/common/flat_app.cpp
        src/common/flat_style.cpp src/common/log.cpp src/injector/launcher_ui.cpp
        src/overlay/control_panel.cpp)
    target_include_directories(mcov_launcher_window_check PRIVATE src third_party/glass)
    target_link_libraries(mcov_launcher_window_check PRIVATE mcov_settings mcov_liquid d3d11 dxgi dwmapi gdi32 ole32 windowscodecs)
    add_test(NAME launcher_window COMMAND mcov_launcher_window_check "${CMAKE_BINARY_DIR}/ui-preview")
    set_tests_properties(launcher_window PROPERTIES TIMEOUT 30)
    add_executable(mcov_ui_check
        "${CMAKE_CURRENT_BINARY_DIR}/menu_fonts.rc"
        tests/flat_style_smoke.cpp
        src/common/flat_style.cpp
        src/common/log.cpp)
    target_include_directories(mcov_ui_check PRIVATE src third_party/glass)
    target_link_libraries(mcov_ui_check PRIVATE mcov_settings imgui)
    add_test(NAME flat_ui COMMAND mcov_ui_check)
    add_executable(mcov_client_controls_check tests/client_controls.cpp)
    target_include_directories(mcov_client_controls_check PRIVATE src)
    target_link_libraries(mcov_client_controls_check PRIVATE mcov_settings imgui)
    add_test(NAME client_controls COMMAND mcov_client_controls_check)
    add_executable(mcov_item_assist_check tests/item_assist.cpp)
    target_include_directories(mcov_item_assist_check PRIVATE src)
    target_link_libraries(mcov_item_assist_check PRIVATE mcov_settings)
    add_test(NAME item_assist COMMAND mcov_item_assist_check)
    add_executable(mcov_world_visuals_check tests/world_visuals.cpp src/overlay/world_visuals.cpp)
    target_include_directories(mcov_world_visuals_check PRIVATE src)
    target_link_libraries(mcov_world_visuals_check PRIVATE mcov_settings imgui)
    add_test(NAME world_visuals COMMAND mcov_world_visuals_check)
    add_executable(mcov_smp_check tests/smp_features.cpp src/overlay/smp_features.cpp src/overlay/rockstar_features.cpp src/overlay/friend_hat_renderer.cpp)
    target_include_directories(mcov_smp_check PRIVATE src)
    target_link_libraries(mcov_smp_check PRIVATE mcov_settings imgui)
    add_test(NAME smp_features COMMAND mcov_smp_check)
    add_executable(mcov_rockstar_check tests/rockstar_features.cpp src/overlay/rockstar_features.cpp src/overlay/friend_hat_renderer.cpp)
    target_include_directories(mcov_rockstar_check PRIVATE src)
    target_link_libraries(mcov_rockstar_check PRIVATE mcov_settings imgui)
    add_test(NAME rockstar_features COMMAND mcov_rockstar_check)
    add_executable(mcov_control_panel_check
        "${CMAKE_CURRENT_BINARY_DIR}/menu_fonts.rc"
        tests/control_panel_smoke.cpp
        src/injector/launcher_ui.cpp
        src/common/flat_style.cpp src/overlay/control_panel.cpp src/overlay/overlay.cpp src/overlay/esp_renderer.cpp
        src/overlay/visual_draw.cpp
        src/overlay/overlay_modules.cpp
        src/overlay/overlay_input.cpp src/overlay/overlay_hud.cpp
        src/overlay/overlay_configs.cpp
        src/overlay/overlay_world_visuals.cpp
        src/overlay/overlay_rockstar.cpp src/overlay/rockstar_features.cpp
    src/overlay/overlay_utilities.cpp
        src/overlay/overlay_smp.cpp
        src/overlay/smp_features.cpp
        src/overlay/combat_menu.cpp
        src/overlay/item_icon_atlas_dx11.cpp
        src/overlay/aura_renderer.cpp src/overlay/aura_blend_dx11.cpp src/overlay/depth_renderer.cpp
        src/overlay/friend_hat_renderer.cpp
        src/overlay/target_cube_renderer.cpp
        src/overlay/world_visuals.cpp
        src/overlay/process.cpp src/common/ipc.cpp src/common/log.cpp)
    target_include_directories(mcov_control_panel_check PRIVATE src third_party/glass)
    target_link_libraries(mcov_control_panel_check PRIVATE mcov_settings imgui
        mcov_liquid
        user32 gdi32 shell32 dwmapi ole32 windowscodecs winmm)
    add_test(NAME control_panel COMMAND mcov_control_panel_check "${CMAKE_BINARY_DIR}/ui-preview")
    set_tests_properties(control_panel PROPERTIES TIMEOUT 30)
    add_executable(mcov_aura_check tests/aura_smoke.cpp src/overlay/aura_renderer.cpp src/overlay/aura_blend_dx11.cpp
        src/overlay/friend_hat_renderer.cpp src/overlay/target_cube_renderer.cpp)
    target_include_directories(mcov_aura_check PRIVATE src)
    target_link_libraries(mcov_aura_check PRIVATE mcov_settings imgui)
    add_test(NAME aura_rendering COMMAND mcov_aura_check)
    add_executable(mcov_depth_check tests/depth_rendering.cpp src/overlay/depth_renderer.cpp src/overlay/smp_features.cpp)
    target_include_directories(mcov_depth_check PRIVATE src)
    target_link_libraries(mcov_depth_check PRIVATE mcov_settings imgui)
    add_test(NAME depth_rendering COMMAND mcov_depth_check)
    add_executable(mcov_depth_gl_check tests/depth_capture_gl.cpp src/agent/jvm_interface.cpp src/common/log.cpp)
    target_include_directories(mcov_depth_gl_check PRIVATE src ${JNI_INCLUDE_DIRS})
    target_link_libraries(mcov_depth_gl_check PRIVATE mcov_settings opengl32 user32 gdi32)
    add_test(NAME depth_capture_gl COMMAND mcov_depth_gl_check)
    set_tests_properties(depth_capture_gl PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_executable(mcov_base_gl_check tests/base_texture_gl.cpp src/agent/base_texture_gl.cpp src/common/log.cpp)
    target_include_directories(mcov_base_gl_check PRIVATE src)
    target_link_libraries(mcov_base_gl_check PRIVATE mcov_settings opengl32 user32 gdi32)
    add_test(NAME base_texture_gl COMMAND mcov_base_gl_check)
    set_tests_properties(base_texture_gl PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    set_tests_properties(aura_rendering PROPERTIES TIMEOUT 30)
endif()

option(MCOV_BUILD_COMPAT_CHECKS "Build JNI profile and attach-runtime regression checks" OFF)
if(MCOV_BUILD_COMPAT_CHECKS)
    enable_testing()
    find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
    add_test(NAME namespace_inheritance COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/namespace_inheritance.py")
    add_executable(mcov_agent_package_match_check tests/agent_package_match.cpp src/injector/portable_package.cpp src/overlay/process.cpp src/common/ipc.cpp src/common/log.cpp)
    target_include_directories(mcov_agent_package_match_check PRIVATE src)
    target_link_libraries(mcov_agent_package_match_check PRIVATE mcov_settings bcrypt user32)
    add_test(NAME agent_package_match COMMAND mcov_agent_package_match_check "${CMAKE_BINARY_DIR}/agent-package-match")
    set(MCOV_BASE_TEST_DIR ${CMAKE_BINARY_DIR}/base-texture-test)
    add_custom_command(OUTPUT ${MCOV_BASE_TEST_DIR}/mcov/BaseTextureCheck.class
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MCOV_BASE_TEST_DIR}
        COMMAND ${Java_JAVAC_EXECUTABLE} --release 17 -d ${MCOV_BASE_TEST_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/BaseTextureCheck.java
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/frame-sync/BaseTextureBridge.java
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/frame-sync/BaseTextureProfiles.java
        DEPENDS tests/BaseTextureCheck.java tools/frame-sync/BaseTextureBridge.java tools/frame-sync/BaseTextureProfiles.java VERBATIM)
    add_custom_target(mcov_base_texture_check ALL DEPENDS ${MCOV_BASE_TEST_DIR}/mcov/BaseTextureCheck.class)
    add_test(NAME base_texture_models COMMAND ${Java_JAVA_EXECUTABLE} -cp ${MCOV_BASE_TEST_DIR} mcov.BaseTextureCheck)
    add_executable(mcov_base_texture_probe tools/diagnostics/base_texture_probe.cpp src/common/ipc.cpp src/common/log.cpp)
    target_include_directories(mcov_base_texture_probe PRIVATE src)
    target_link_libraries(mcov_base_texture_probe PRIVATE mcov_settings)
    add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/compat-fixtures/.stamp"
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/compatibility/make_fixtures.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/agent/generated_profiles.hpp" "${CMAKE_BINARY_DIR}/compat-fixtures"
        COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/compat-fixtures/.stamp"
        DEPENDS src/agent/generated_profiles.hpp tools/compatibility/make_fixtures.py
            tools/compatibility/generate_profiles.py
            tools/compatibility/geometry_schema.py
        VERBATIM)
    add_custom_target(mcov_compat_fixtures DEPENDS "${CMAKE_BINARY_DIR}/compat-fixtures/.stamp")
    add_executable(mcov_compat_check tests/runtime_profiles.cpp src/agent/jvm_interface.cpp src/agent/base_texture_gl.cpp
        src/overlay/process.cpp src/common/ipc.cpp src/common/log.cpp)
    target_include_directories(mcov_compat_check PRIVATE src ${JNI_INCLUDE_DIRS})
    target_link_libraries(mcov_compat_check PRIVATE mcov_settings user32 opengl32)
    add_dependencies(mcov_compat_check mcov_compat_fixtures mcov_frame_agent)
    add_test(NAME runtime_profiles COMMAND mcov_compat_check "${MCOV_JDK_ROOT}" "${CMAKE_BINARY_DIR}/compat-fixtures")
    add_test(NAME portable_client COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/portable_client.py" "$<TARGET_FILE:mcov>"
        "$<TARGET_FILE_DIR:mcov_overlay>" "${CMAKE_BINARY_DIR}/portable-check" "${MCOV_CLIENT_ARCHIVE}")
    set_tests_properties(runtime_profiles PROPERTIES TIMEOUT 120)
    set(MCOV_FRAME_TEST_DIR ${CMAKE_BINARY_DIR}/frame-sync-test)
    add_custom_command(OUTPUT ${MCOV_FRAME_TEST_DIR}/mcov/sync/FrameSyncCheck.class
            ${MCOV_FRAME_TEST_DIR}/mcov/sync/FrameSyncAttachCheck.class
            ${MCOV_FRAME_TEST_DIR}/mcov/sync/DepthHookCheck.class
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MCOV_FRAME_TEST_DIR}
        COMMAND ${Java_JAVAC_EXECUTABLE} -source 17 -target 17
            --add-exports java.base/jdk.internal.org.objectweb.asm=ALL-UNNAMED -d ${MCOV_FRAME_TEST_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/FrameSyncCheck.java
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/FrameSyncAttachCheck.java
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/DepthHookCheck.java
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/frame-sync/FrameSignal.java
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/frame-sync/FrameProfiles.java
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/frame-sync/DepthProfiles.java
        DEPENDS mcov_frame_agent tests/FrameSyncCheck.java tests/FrameSyncAttachCheck.java tests/DepthHookCheck.java
            tools/frame-sync/FrameProfiles.java tools/frame-sync/DepthProfiles.java tools/frame-sync/FrameSignal.java
        VERBATIM)
    add_custom_target(mcov_frame_check ALL DEPENDS ${MCOV_FRAME_TEST_DIR}/mcov/sync/FrameSyncCheck.class
            ${MCOV_FRAME_TEST_DIR}/mcov/sync/FrameSyncAttachCheck.class ${MCOV_FRAME_TEST_DIR}/mcov/sync/DepthHookCheck.class)
    add_dependencies(mcov_frame_check mcov_compat_fixtures)
    add_test(NAME frame_sync COMMAND ${Java_JAVA_EXECUTABLE} "-javaagent:${MCOV_TOOLS_DIR}/frame-sync-agent.jar"
        -cp "${MCOV_FRAME_TEST_DIR};${MCOV_TOOLS_DIR}/frame-sync-agent.jar"
        mcov.sync.FrameSyncCheck "${CMAKE_BINARY_DIR}/compat-fixtures")
    add_test(NAME frame_attach COMMAND ${Java_JAVA_EXECUTABLE} -Djdk.attach.allowAttachSelf=true
        -cp "${MCOV_FRAME_TEST_DIR};${MCOV_TOOLS_DIR}/frame-sync-agent.jar"
        mcov.sync.FrameSyncAttachCheck "${CMAKE_BINARY_DIR}/compat-fixtures" "${MCOV_TOOLS_DIR}/frame-sync-agent.jar")
    set_tests_properties(frame_attach PROPERTIES TIMEOUT 30)
    add_test(NAME depth_hook COMMAND ${Java_JAVA_EXECUTABLE}
        --add-exports java.base/jdk.internal.org.objectweb.asm=ALL-UNNAMED
        "-javaagent:${MCOV_TOOLS_DIR}/frame-sync-agent.jar"
        -cp "${MCOV_FRAME_TEST_DIR};${MCOV_TOOLS_DIR}/frame-sync-agent.jar" mcov.sync.DepthHookCheck)
endif()

option(MCOV_BUILD_ESP_CHECKS "Build camera projection and world-space rig checks" OFF)
if(MCOV_BUILD_ESP_CHECKS)
    enable_testing()
    add_executable(mcov_esp_check tests/esp_geometry.cpp)
    target_link_libraries(mcov_esp_check PRIVATE mcov_settings)
    add_test(NAME esp_geometry COMMAND mcov_esp_check)
    add_executable(mcov_esp_render_check
        "${CMAKE_CURRENT_BINARY_DIR}/menu_fonts.rc"
        tests/esp_rendering.cpp src/overlay/esp_renderer.cpp src/overlay/visual_draw.cpp
        src/common/flat_style.cpp src/common/log.cpp)
    target_include_directories(mcov_esp_render_check PRIVATE src third_party/glass)
    target_link_libraries(mcov_esp_render_check PRIVATE mcov_settings imgui)
    add_test(NAME esp_rendering COMMAND mcov_esp_render_check "${CMAKE_BINARY_DIR}/visual-preview/esp.bmp")
    set_tests_properties(esp_rendering PROPERTIES TIMEOUT 30)
endif()

add_executable(mcov_combat_demo tools/combat_demo.cpp)
target_link_libraries(mcov_combat_demo PRIVATE mcov_settings mcov_combat)
add_library(mcov_combat_hud STATIC src/combat/debug_hud.cpp)
target_link_libraries(mcov_combat_hud PUBLIC mcov_combat imgui mcov_settings)
target_link_libraries(mcov_overlay PRIVATE mcov_combat_hud)
if(TARGET mcov_control_panel_check)
    target_link_libraries(mcov_control_panel_check PRIVATE mcov_combat_hud)
endif()
option(MCOV_BUILD_COMBAT_CHECKS "Build deterministic Combat Assist checks" ON)
if(MCOV_BUILD_COMBAT_CHECKS)
    enable_testing()
    add_executable(mcov_combat_check tests/combat_assist.cpp)
    target_link_libraries(mcov_combat_check PRIVATE mcov_settings mcov_combat)
    add_test(NAME combat_assist COMMAND mcov_combat_check)
    add_executable(mcov_spinbot_check tests/spinbot.cpp)
    target_include_directories(mcov_spinbot_check PRIVATE src)
    add_test(NAME spinbot COMMAND mcov_spinbot_check)
    add_executable(mcov_freecam_check tests/freecam.cpp)
    target_include_directories(mcov_freecam_check PRIVATE src)
    target_link_libraries(mcov_freecam_check PRIVATE mcov_settings imgui)
    add_test(NAME freecam COMMAND mcov_freecam_check)
    add_test(NAME combat_demo COMMAND mcov_combat_demo)
    add_executable(mcov_combat_hud_check tests/combat_hud.cpp)
    target_link_libraries(mcov_combat_hud_check PRIVATE mcov_combat_hud)
    add_test(NAME combat_hud COMMAND mcov_combat_hud_check)
endif()
