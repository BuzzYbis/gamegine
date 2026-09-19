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
-- Continuous Integration tasks
--
-- 'xmake ci-check' runs the push tier locally, exactly as the runner does.
-- See ci/README.md for the job tiers and what each task enforces.

includes("ci/tasks.lua")


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

-- Local package definitions, for dependencies pinned to an exact commit
-- rather than to a published release. See xmake/packages/ and ci/pins.json.
add_repositories("gamegine-packages xmake", {rootdir = os.scriptdir()})

-- Vulkan SDK (LunarG / system Vulkan loader & headers)
add_requires("vulkansdk")

-- Fast glTF 2.0 parser (https://github.com/spnda/fastgltf)
add_requires("fastgltf")

-- Google Test & Mock framework (gmock is enabled by default;
-- "main" links the gtest_main entry point used by 'xmake test')
add_requires("gtest", {configs = {main = true}})

-- Google Benchmark framework
add_requires("benchmark")

-- Mesh optimization: clusterization, simplification and codecs (D0).
-- Pinned to an exact commit because clusterlod.h needs APIs that no release
-- carries yet; see xmake/packages/m/meshoptimizer/xmake.lua for why.
add_requires("meshoptimizer 7d8eca58818927c6b2dbcdd44763f95b798d4232")


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
    add_packages("vulkansdk", "fastgltf", "meshoptimizer", {public = true})

    -- clusterlod.h is a single header vendored from the upstream demo/
    -- directory at the same commit as meshoptimizer. It is integrated, not
    -- authored (D0), and one translation unit must define
    -- CLUSTERLOD_IMPLEMENTATION before including it.
    add_includedirs("third_party/clusterlod", {public = true})

    -- Cross-platform configuration:
    -- 1. macOS (Apple Silicon / arm64) using MoltenVK / Metal
    if is_plat("macosx") then
        add_frameworks("Metal", "Foundation", "QuartzCore", "Cocoa", "IOKit", "CoreVideo")
        add_defines("VK_USE_PLATFORM_METAL_EXT", {public = true})

        -- The LunarG loader ships with '@rpath/libvulkan.1.dylib' as its install
        -- name, so every executable linking it needs an rpath to the SDK, else
        -- it builds fine but dies at load time with "Library not loaded".
        local vulkan_sdk = os.getenv("VULKAN_SDK")
        if vulkan_sdk then
            add_rpathdirs(path.join(vulkan_sdk, "lib"), {public = true})
        end
        -- Default location used by the LunarG system-wide installer.
        add_rpathdirs("/usr/local/lib", {public = true})
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
    add_packages("gtest")

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


-- =============================================================================
-- Sample Assets (glTF scenes fetched on demand)
--
-- The sample scenes are too large for the repository, so they live on a public
-- kDrive share and are downloaded once, on demand, into sample/models.
--
--   xmake build sample-models
--
-- Any target needing the assets can either add the rule directly or simply
-- declare add_deps("sample-models").

rule("gamegine.sample_models")
    before_build(function (target)
        import("lib.detect.find_tool")

        -- Archives to fetch: <file name> = <direct download url>
        local archives = {
            ["bistro.zip"] = "https://kdrive.infomaniak.com/2/app/1926560/share/540dc381-0262-4a23-947b-7a5fc11790b3/files/11063/download"
        }

        local modelsdir = path.join(os.projectdir(), "sample", "models")
        for archive, url in pairs(archives) do

            -- The extracted scene lives in a directory named after the archive.
            local outdir = path.join(modelsdir, path.basename(archive))
            if os.isdir(outdir) then
                goto continue
            end

            local curl = assert(find_tool("curl"), "curl not found, it is required to fetch %s", archive)
            local unzip = assert(find_tool("unzip"), "unzip not found, it is required to extract %s", archive)

            local zipfile = path.join(os.tmpdir(), archive)
            local stagedir = os.tmpfile() .. ".unzip"
            try
            {
                function ()
                    cprint("${color.build.target}downloading %s ..", archive)
                    os.vrunv(curl.program, {"-fL", "--progress-bar", "-o", zipfile, url})

                    -- '__MACOSX' holds macOS resource forks, useless to the engine.
                    cprint("${color.build.target}extracting %s -> %s", archive, outdir)
                    os.vrunv(unzip.program, {"-q", "-o", zipfile, "-x", "__MACOSX/*", "-d", stagedir})

                    -- Drop the archive's own root folder, if it has one, so the
                    -- scene is never nested twice (sample/models/bistro/bistro).
                    local dirs = os.dirs(path.join(stagedir, "*"))
                    local files = os.files(path.join(stagedir, "*"))
                    os.mkdir(modelsdir)
                    if #dirs == 1 and #files == 0 then
                        os.mv(dirs[1], outdir)
                    else
                        os.mv(stagedir, outdir)
                    end
                end,
                catch
                {
                    function (errors)
                        os.tryrm(outdir)
                        raise("failed to fetch the sample model %s: %s", archive, errors)
                    end
                },
                finally
                {
                    function ()
                        os.tryrm(zipfile)
                        os.tryrm(stagedir)
                    end
                }
            }

            ::continue::
        end
    end)
rule_end()


-- =============================================================================
-- Target: Sample Models (asset download only, builds nothing)

target("sample-models")
    set_kind("phony")
    set_default(false)
    add_rules("gamegine.sample_models")
