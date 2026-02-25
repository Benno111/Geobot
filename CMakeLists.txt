cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(geobot2 VERSION 1.0.0)

option(geobot_ENABLE_TEST_BUILDS "Enable test targets in this project and dependencies" OFF)
set(BUILD_TESTING ${geobot_ENABLE_TEST_BUILDS} CACHE BOOL "Build tests" FORCE)
 
add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/global.cpp
    src/macro.cpp
    src/keybinds.cpp
    src/utils/utils.cpp
    src/gdr/gdr.cpp
    src/renderer/renderer.cpp

    src/hacks/other.cpp
    src/hacks/tps_bypass.cpp
    src/hacks/frame_stepper.cpp
    src/hacks/layout_mode.cpp
    src/hacks/show_trajectory.cpp
    src/hacks/coin_finder.cpp
    src/hacks/clickbot.cpp
    src/hacks/autoclicker.cpp

    src/ui/macro_editor.cpp
    src/ui/button_edit_layer.cpp
    src/ui/game_ui.cpp
    src/ui/record_layer.cpp
    src/ui/render_settings_layer.cpp
    src/ui/load_macro_layer.cpp
    src/ui/clickbot_layer.cpp
    src/ui/button_setting.cpp

    src/practice_fixes/input.cpp
    src/practice_fixes/player.cpp
    src/practice_fixes/play_layer.cpp
)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

if (MSVC)
    # Some MSVC toolsets report _MSVC_LANG as 202004L under /std:c++latest.
    # Geode currently hard-requires 202302L, so patch the local SDK check for MSVC.
    set(_geode_platform_hpp "$ENV{GEODE_SDK}/loader/include/Geode/platform/platform.hpp")
    set(_geode_event_hpp "$ENV{GEODE_SDK}/loader/include/Geode/loader/Event.hpp")
    if (EXISTS "${_geode_platform_hpp}")
        file(READ "${_geode_platform_hpp}" _geode_platform_content)
        set(_geode_platform_original "${_geode_platform_content}")
        string(REPLACE
            "GEODE_CXX_STANDARD >= 202302L,"
            "GEODE_CXX_STANDARD >= 202004L,"
            _geode_platform_content
            "${_geode_platform_content}"
        )
        if (NOT _geode_platform_content STREQUAL _geode_platform_original)
            file(WRITE "${_geode_platform_hpp}" "${_geode_platform_content}")
            message(STATUS "Applied MSVC compatibility patch to Geode platform.hpp C++ standard check")
        endif()
    endif()
    if (EXISTS "${_geode_event_hpp}")
        file(READ "${_geode_event_hpp}" _geode_event_content)
        set(_geode_event_original "${_geode_event_content}")
        string(REPLACE
            "bool send(PArgs... args) noexcept(std::is_nothrow_invocable_v<geode::CopyableFunction<PReturn(PArgs...)>, PArgs...>);"
            "bool send(PArgs... args);"
            _geode_event_content
            "${_geode_event_content}"
        )
        string(REPLACE
            "::send(PArgs... args) noexcept(std::is_nothrow_invocable_v<geode::CopyableFunction<PReturn(PArgs...)>, PArgs...>) {"
            "::send(PArgs... args) {"
            _geode_event_content
            "${_geode_event_content}"
        )
        if (NOT _geode_event_content STREQUAL _geode_event_original)
            file(WRITE "${_geode_event_hpp}" "${_geode_event_content}")
            message(STATUS "Applied MSVC compatibility patch to Geode Event.hpp send() noexcept")
        endif()
    endif()
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

if (MSVC)
    # ARC v1.5.3 uses a few C++23 constructs that still fail on some MSVC toolchains.
    # Patch vendored headers in build/_deps so builds are reproducible without manual edits.
    set(_arc_future_dir "${CMAKE_CURRENT_BINARY_DIR}/_deps/arc-src/include/arc/future")
    set(_arc_pollable_hpp "${_arc_future_dir}/Pollable.hpp")
    set(_arc_metadata_hpp "${_arc_future_dir}/PollableMetadata.hpp")
    set(_arc_future_hpp "${_arc_future_dir}/Future.hpp")

    if (EXISTS "${_arc_pollable_hpp}")
        file(READ "${_arc_pollable_hpp}" _arc_pollable_content)
        set(_arc_pollable_original "${_arc_pollable_content}")
        string(REPLACE
            "bool await_suspend(this auto& self, std::coroutine_handle<> h) noexcept(NothrowPoll) {\n        return self.template fastSuspend<Derived>(h);\n    }"
            "bool await_suspend(std::coroutine_handle<> h) noexcept(NothrowPoll) {\n        return this->template fastSuspend<Derived>(h);\n    }"
            _arc_pollable_content
            "${_arc_pollable_content}"
        )
        string(REPLACE
            "bool fastSuspend(this Derived& self, std::coroutine_handle<> h) {\n        constexpr bool IsNothrowPollable = ::arc::IsNothrowPollable<Derived>;\n\n        auto cx = contextFromHandle(h);\n        bool ready = self.template vPoll<IsNothrowPollable>(&self, *cx);\n        if (ready) {\n            // fast path - no need to attach to parent\n            return false;\n        }\n\n        // attach to parent and suspend\n        self.attachToParent(h);\n        return true;\n    }"
            "bool fastSuspend(std::coroutine_handle<> h) {\n        constexpr bool IsNothrowPollable = ::arc::IsNothrowPollable<Derived>;\n\n        auto* self = static_cast<Derived*>(this);\n        auto cx = contextFromHandle(h);\n        bool ready = self->template vPoll<IsNothrowPollable>(self, *cx);\n        if (ready) {\n            // fast path - no need to attach to parent\n            return false;\n        }\n\n        // attach to parent and suspend\n        self->attachToParent(h);\n        return true;\n    }"
            _arc_pollable_content
            "${_arc_pollable_content}"
        )
        string(REPLACE
            "constexpr auto meta = PollableMetadata::create<Derived>();"
            "const auto meta = PollableMetadata::create<Derived>();"
            _arc_pollable_content
            "${_arc_pollable_content}"
        )
        string(REPLACE
            "inline static constinit const PollableVtable vtable = [] {"
            "inline static const PollableVtable vtable = [] {"
            _arc_pollable_content
            "${_arc_pollable_content}"
        )
        if (NOT _arc_pollable_content STREQUAL _arc_pollable_original)
            file(WRITE "${_arc_pollable_hpp}" "${_arc_pollable_content}")
            message(STATUS "Applied MSVC compatibility patch to arc/future/Pollable.hpp")
        endif()
    endif()

    if (EXISTS "${_arc_metadata_hpp}")
        file(READ "${_arc_metadata_hpp}" _arc_metadata_content)
        set(_arc_metadata_original "${_arc_metadata_content}")
        string(REPLACE
            "static constexpr const PollableMetadata* create() {"
            "static const PollableMetadata* create() {"
            _arc_metadata_content
            "${_arc_metadata_content}"
        )
        string(REPLACE
            "static constexpr auto value = [] {"
            "static const auto value = [] {"
            _arc_metadata_content
            "${_arc_metadata_content}"
        )
        if (NOT _arc_metadata_content STREQUAL _arc_metadata_original)
            file(WRITE "${_arc_metadata_hpp}" "${_arc_metadata_content}")
            message(STATUS "Applied MSVC compatibility patch to arc/future/PollableMetadata.hpp")
        endif()
    endif()

    if (EXISTS "${_arc_future_hpp}")
        file(READ "${_arc_future_hpp}" _arc_future_content)
        set(_arc_future_original "${_arc_future_content}")
        string(REPLACE
            "static constexpr PollableVtable vtable {"
            "inline static const PollableVtable vtable {"
            _arc_future_content
            "${_arc_future_content}"
        )
        string(REPLACE
            "auto resume = [&] noexcept {"
            "auto resume = [&] {"
            _arc_future_content
            "${_arc_future_content}"
        )
        if (NOT _arc_future_content STREQUAL _arc_future_original)
            file(WRITE "${_arc_future_hpp}" "${_arc_future_content}")
            message(STATUS "Applied MSVC compatibility patch to arc/future/Future.hpp")
        endif()
    endif()
endif()

if (WIN32)
    # bindings 2.2081 misses <sys/timeb.h> in a couple of inline files that use _ftime64_s.
    # Re-apply this fix on configure so Codegen updates don't reintroduce build breaks.
    set(_bindings_inline_dir "${CMAKE_CURRENT_BINARY_DIR}/_deps/bindings-src/bindings/2.2081/inline")
    foreach(_inline_file DialogLayer.cpp GameStatsManager.cpp)
        set(_inline_path "${_bindings_inline_dir}/${_inline_file}")
        if (EXISTS "${_inline_path}")
            file(READ "${_inline_path}" _inline_content)
            if (_inline_content MATCHES "__timeb64|_ftime64_s")
                if (NOT _inline_content MATCHES "#include <sys/timeb.h>")
                    string(REPLACE
                        "#include <Geode/Bindings.hpp>\n"
                        "#include <Geode/Bindings.hpp>\n\n#if defined(GEODE_IS_WINDOWS)\n#include <sys/timeb.h>\n#endif\n"
                        _inline_content
                        "${_inline_content}"
                    )
                    file(WRITE "${_inline_path}" "${_inline_content}")
                    message(STATUS "Applied MSVC compatibility patch to ${_inline_file} (sys/timeb.h include)")
                endif()
            endif()
        endif()
    endforeach()
endif()
if (TARGET GeodeBindings)
    # MSVC 19.42 can ICE in Geode Event.hpp under Unity builds and also emit
    # a large amount of duplicate object-name warnings in this target.
    set_target_properties(GeodeBindings PROPERTIES UNITY_BUILD OFF)
    if (MSVC)
        target_compile_options(GeodeBindings PRIVATE
            /FS
            $<$<CONFIG:Release>:/Od /Ob0>
            $<$<CONFIG:RelWithDebInfo>:/Od /Ob0>
            $<$<CONFIG:MinSizeRel>:/Od /Ob0>
        )
    endif()
endif()

setup_geode_mod(${PROJECT_NAME})
