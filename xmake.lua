-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
-- =============================================================================
-- Gamegine 
-- Build Configuration
-- =============================================================================

set_project("gamegine")
set_version("0.1.0")
set_description("A Game Engine using Vulkan and C++23")

set_languages("cxx23")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

add_rules("mode.debug", "mode.release")


-- =============================================================================
-- Sanitizers Configuration
--
-- Can be enabled via dedicated modes:
--   xmake f -m asan  (AddressSanitizer + UndefinedBehaviorSanitizer)
--   xmake f -m ubsan (UndefinedBehaviorSanitizer)
--   xmake f -m tsan  (ThreadSanitizer)
--
-- Or via individual configuration flags:
--   xmake f --asan=y
--   xmake f --ubsan=y
--   xmake f --tsan=y

option("asan")
    set_default(false)
    set_showmenu(true)
    set_category("sanitizers")
    set_description("Enable AddressSanitizer (ASAN)")
option_end()

option("ubsan")
    set_default(false)
    set_showmenu(true)
    set_category("sanitizers")
    set_description("Enable UndefinedBehaviorSanitizer (UBSAN)")
option_end()

option("tsan")
    set_default(false)
    set_showmenu(true)
    set_category("sanitizers")
    set_description("Enable ThreadSanitizer (TSAN)")
option_end()

if is_mode("asan") or has_config("asan") then
    set_policy("build.sanitizer.address", true)
    set_symbols("debug")
    set_optimize("none")
end

if is_mode("ubsan") or has_config("ubsan") then
    set_policy("build.sanitizer.undefined", true)
    set_symbols("debug")
    set_optimize("none")
end

if is_mode("tsan") or has_config("tsan") then
    set_policy("build.sanitizer.thread", true)
    set_symbols("debug")
    set_optimize("none")
end


-- =============================================================================
-- Compiler Flags

rule("gamegine.warnings")
    on_load(function (target)
        target:add("cxxflags",
            -- Enable common / standard warning diagnostics.
            "-Wall",
            -- Enable additional warning diagnostics not covered by -Wall.
            "-Wextra",
            -- Enforce strict ISO C++ compliance and reject non-standard extensions.
            "-Wpedantic",
            -- Warn when a variable shadows another variable in an outer scope.
            "-Wshadow",  
            -- Warn if a class with virtual functions lacks a virtual destructor.
            "-Wnon-virtual-dtor",
            -- Warn on C-style casts in C++.
            "-Wold-style-cast",
            -- Warn when pointer casting increases required memory alignment.
            "-Wcast-align",
            -- Warn on unused variables, functions, parameters, or expressions.
            "-Wunused", 
            -- Warn when a function hides a virtual function from a base class.
            "-Woverloaded-virtual",
            -- Warn on implicit type conversions that may alter values or lose precision.
            "-Wconversion",
            -- Warn on implicit conversions between signed and unsigned types.
            "-Wsign-conversion",
            {tools = {"clang", "gcc", "apple_clang"}}
        )
    end)
rule_end()

rule("gamegine.werror")
    on_load(function (target)
        target:add("cxxflags",
            -- Treat all warnings as compilation errors.
            "-Werror",
            {tools = {"clang", "gcc", "apple_clang"}}
        )
    end)
rule_end()


-- =============================================================================
-- Package Dependencies

-- Vulkan SDK (LunarG / system Vulkan loader & headers)
add_requires("vulkansdk")

-- Fast glTF 2.0 parser (https://github.com/spnda/fastgltf)
add_requires("fastgltf")

-- Google Test & Mock framework
add_requires("googletest")

-- Google Benchmark framework
add_requires("benchmark")


-- =============================================================================
-- Target: Gamegine (Static Library)

target("gamegine")
    set_kind("static")
    add_rules("gamegine.warnings", "gamegine.werror")

    -- Include directories & headers
    add_includedirs("gamegine/include", {public = true})
    add_headerfiles("gamegine/include/(engine/**.h)")

    -- Source files
    add_files("gamegine/src/**.cpp")

    -- Dependencies
    add_packages("vulkansdk", "fastgltf", {public = true})

    -- Cross-platform configuration:
    -- 1. macOS (Apple Silicon / arm64) using MoltenVK / Metal
    if is_plat("macosx") then
        add_frameworks("Metal", "Foundation", "QuartzCore", "Cocoa", "IOKit", "CoreVideo")
        add_defines("VK_USE_PLATFORM_METAL_EXT", {public = true})
    -- 2. Linux (x86_64) using native NVIDIA GPU Vulkan driver
    elseif is_plat("linux") then
        add_syslinks("pthread", "dl", "m")
        add_defines("VK_USE_PLATFORM_XLIB_KHR", "VK_USE_PLATFORM_WAYLAND_KHR", {public = true})
    end


-- =============================================================================
-- Target: Tests (Unit & Integration Tests via Google Test)

target("tests")
    set_kind("binary")
    set_default(false)
    add_rules("gamegine.warnings")

    -- Engine dependency
    add_deps("gamegine")

    -- Test sources
    add_files("tests/**.cpp")

    -- Google Test dependency
    add_packages("googletest")

    -- Enable test discovery via 'xmake test'
    add_tests("unit")


-- =============================================================================
-- Target: Benchmarks (Performance Benchmarks via Google Benchmark)

target("benchmarks")
    set_kind("binary")
    set_default(false)
    add_rules("gamegine.warnings", "gamegine.werror")

    -- Engine dependency
    add_deps("gamegine")

    -- Benchmark sources
    add_files("benchmarks/**.cpp")

    -- Google Benchmark dependency
    add_packages("benchmark")


-- =============================================================================
-- Target: ImGui (Third-Party UI Module)
--
-- Kept separate with suppressed warnings to prevent third-party code from
-- violating the strict first-party -Werror policy.

target("imgui")
    set_kind("static")
    set_default(false)

    add_files("third_party/imgui/imgui*.cpp")
    add_includedirs("third_party/imgui", {public = true})

    -- Disable warnings for third-party code
    set_warnings("none")

    if is_plat("macosx") then
        add_frameworks("Metal", "Cocoa")
    end
