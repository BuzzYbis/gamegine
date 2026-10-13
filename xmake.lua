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

-- 'lsp = "clangd"' keeps third-party headers (gtest, the Vulkan SDK,
-- fastgltf...) as '-isystem' in compile_commands.json, as the build uses
-- them. Without it xmake writes them as '-I', for editors that cannot read
-- '-isystem', and clang-tidy then reports gtest's own macros -- every
-- EXPECT_DEATH -- as findings in our tests. clangd reads '-isystem'.
add_rules("plugin.compile_commands.autoupdate",
          {outputdir = ".", lsp = "clangd"})

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

-- D9: errors are values. First-party code neither throws nor catches, and
-- '-fno-exceptions' makes that a compile error rather than a convention. The
-- dependencies were checked before this was adopted: fastgltf guards its one
-- 'raise()' on __cpp_exceptions, meshoptimizer is a C API, and GoogleTest
-- builds with GTEST_HAS_EXCEPTIONS=0 (losing only EXPECT_THROW, which a
-- project that never throws has no use for).
rule("gamegine.no_exceptions")
    on_load(function (target)
        target:add("cxxflags", "-fno-exceptions",
                   {tools = {"clang", "gcc", "apple_clang"}})
        target:add("defines", "GTEST_HAS_EXCEPTIONS=0")
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

-- vg_bench reads the shader index, build/shaders/index.json.
add_requires("nlohmann_json v3.12.0")

-- vg_bench checks each shader artifact against the index's sha256.
add_requires("picosha2 2022.08.08")

-- vg_bench's Metal backend: Apple's C++ interface to Metal, so the backend
-- is plain C++ rather than Objective-C++. Version 26 matches the macOS 26
-- SDK.
if is_plat("macosx") then
    add_requires("metal-cpp 26")
end



-- =============================================================================
-- Target: Gamegine (Static Library)

target("gamegine")
    -- The library is static once it has translation units. Until then it is
    -- header-only: 'ar' cannot archive zero objects, and the first components
    -- (core_error.h) are header-only by design. This lets the tests build and
    -- run against headers before the first .cpp lands, instead of the test
    -- environment being unprovable until then.
    if #os.files(path.join(os.scriptdir(), "gamegine/src/**.cpp")) > 0 then
        set_kind("static")
    else
        set_kind("headeronly")
    end

    add_rules("gamegine.warnings", "gamegine.werror", "gamegine.no_exceptions")

    -- Include directories & headers, once the directory exists.
    if os.isdir(path.join(os.scriptdir(), "gamegine/include")) then
        add_includedirs("gamegine/include", {public = true})
        add_headerfiles("gamegine/include/(engine/**.h)")
    end

    -- Source files. The suffix conventions are:
    --
    --   <component>.cpp     library source
    --   <component>.t.cpp   unit test
    --   <component>.b.cpp   benchmark
    --   <component>.m.cpp   an executable's main
    --
    -- so a test or benchmark sitting beside its component is never archived
    -- into the library by accident.
    --
    -- Guarded for the same reason as the kind above: xmake warns on a
    -- pattern that matches nothing, and a warning that is expected is a
    -- warning people learn to ignore.
    if #os.files(path.join(os.scriptdir(), "gamegine/src/**.cpp")) > 0 then
        add_files("gamegine/src/**.cpp")
        for _, pattern in ipairs({"gamegine/src/**.t.cpp",
                                  "gamegine/src/**.b.cpp",
                                  "gamegine/src/**.m.cpp"}) do
            if #os.files(path.join(os.scriptdir(), pattern)) > 0 then
                remove_files(pattern)
            end
        end
    end

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
    add_rules("gamegine.warnings", "gamegine.no_exceptions")

    -- Engine dependency
    add_deps("gamegine")

    -- Test sources: '.t.cpp' only, wherever they live. A '.cpp' in the test
    -- tree that is not a test is a mistake, and compiling it anyway hides it.
    -- Each pattern is guarded on its own: xmake warns for any pattern that
    -- matches nothing, and colocated tests may exist before the tests tree
    -- does, or the other way round.
    local has_tests = false
    for _, pattern in ipairs({"tests/**.t.cpp", "gamegine/src/**.t.cpp"}) do
        if #os.files(path.join(os.scriptdir(), pattern)) > 0 then
            add_files(pattern)
            has_tests = true
        end
    end

    -- With no test yet there is nothing to link: no object references
    -- gtest_main's main(), so the linker never pulls it in and the binary has
    -- no entry point. Disabled until the first test exists, 'xmake test'
    -- runs nothing instead of failing to link.
    if not has_tests then
        set_enabled(false)
    end

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

    -- Benchmark sources: '.b.cpp' only.
    for _, pattern in ipairs({"benchmarks/**.b.cpp",
                              "gamegine/src/**.b.cpp"}) do
        if #os.files(path.join(os.scriptdir(), pattern)) > 0 then
            add_files(pattern)
        end
    end

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
-- Shaders (Slang, D3)
--
-- Every kernel under shaders/ is compiled ahead of time; nothing is compiled
-- at run time. SPIR-V for Vulkan on Linux; MSL on macOS, put through Apple's
-- metal and metallib so that what ships is what Apple's toolchain accepted.
--
--   xmake build shaders                    compile for this host's target
--   xmake f --shader-targets=spirv,msl     compile for both
--
-- Output lands in build/shaders/, with index.json listing every kernel, its
-- per-target outcome and the shader hash a run manifest records. Anything
-- that loads kernels declares add_deps("shaders").
--
-- The rule and 'xmake ci-shaders' share one implementation,
-- ci/lua/shaders.lua. shaders/README.md has the rules a kernel follows.

option("shader-targets")
    set_default("")
    set_showmenu(true)
    set_category("shaders")
    set_description("Shader targets, comma-separated: spirv, msl.",
                    "Default: spirv on Linux, msl on macOS.")
option_end()

rule("gamegine.slang")
    set_extensions(".slang")

    -- Resolve the targets and probe the toolchain once per build, not once
    -- per file.
    before_build(function (target)
        import("shaders", {rootdir = path.join(os.projectdir(), "ci", "lua")})

        local tc = shaders.toolchain(
            shaders.parse_targets(get_config("shader-targets")))
        shaders.require_toolchain(tc)
        if not tc.slang_pin_matches then
            cprint("${color.warning}slangc %s is not the pinned %s; the "
                   .. "shader hash will not match a pinned build",
                   tc.slangc.value, tc.slang_pin)
        end
        target:data_set("gamegine.slang.toolchain", tc)
    end)

    on_build_file(function (target, sourcefile, opt)
        import("core.project.depend")
        import("utils.progress")
        import("shaders", {rootdir = path.join(os.projectdir(), "ci", "lua")})

        local kernel, reason = shaders.kernel(sourcefile)
        if not kernel then
            raise(reason)
        end
        if kernel.class == "module" then
            return
        end

        local tc = target:data("gamegine.slang.toolchain")
        for _, name in ipairs(tc.targets) do
            local files = shaders.paths(kernel, name)
            local dependfile = target:dependfile(files.record)
            local values = shaders.fingerprint(name, tc)
            local forced = target:is_rebuilt() or not os.isfile(files.record)
            local dependinfo = (not forced and depend.load(dependfile)) or {}
            if forced or depend.is_changed(dependinfo, {
                    lastmtime = os.mtime(dependfile), values = values,
                    files = shaders.inputs(kernel, name)}) then
                progress.show(opt.progress,
                              "${color.build.object}compiling.slang.%s %s",
                              name, sourcefile)
                local record = shaders.compile(kernel, name, {toolchain = tc})
                -- An unsupported probe is a result, not a problem: one line
                -- here, the diagnostics in build/shaders/index.json.
                if record.status == "FAILED" then
                    raise(shaders.describe(record))
                elseif record.status ~= "COMPILED" then
                    cprint("${color.warning}%s [%s] %s${clear}  %s",
                           record.kernel, name, record.status, record.reason)
                end
                -- Saved after the compile, so the imports it just reported
                -- are tracked from the next build on.
                depend.save({files = shaders.inputs(kernel, name),
                             values = values}, dependfile)
            end
        end
    end)

    -- Index what was built, including the kernels that were up to date.
    after_build(function (target)
        import("shaders", {rootdir = path.join(os.projectdir(), "ci", "lua")})

        local kernels = {}
        local batch = target:sourcebatches()["gamegine.slang"]
        for _, sourcefile in ipairs(batch and batch.sourcefiles or {}) do
            local kernel = shaders.kernel(sourcefile)
            if kernel and kernel.class ~= "module" then
                table.insert(kernels, kernel)
            end
        end
        table.sort(kernels, function (a, b) return a.name < b.name end)

        local tc = target:data("gamegine.slang.toolchain")
        local index = shaders.index(kernels, tc,
                                    {generator = "xmake build shaders"})
        shaders.prune(index)
        shaders.write_index(index)
    end)
rule_end()


-- =============================================================================
-- Target: Shaders (compiled kernels, no code)

target("shaders")
    -- 'object', not 'phony': a phony target never runs a rule's per-file
    -- build step.
    set_kind("object")
    set_default(false)
    add_rules("gamegine.slang")

    -- Guarded: xmake warns on a pattern that matches nothing, and R0 starts
    -- with no kernels.
    if #os.files(path.join(os.scriptdir(), "shaders/**.slang")) > 0 then
        add_files("shaders/**.slang")
    end


-- =============================================================================
-- Target: vg_bench (the R0 probe harness)
--
-- One backend per API, bench_vulkan.cpp and bench_metal.cpp, behind the small
-- interface in bench_gpu.h; the probes, the host checks and the report are
-- shared, in vg_bench.cpp, so both APIs run exactly the same work. Headless,
-- and not a renderer (r0-architecture.md section 8).
--
--   xmake build vg_bench
--   xmake run vg_bench --backend=metal

target("vg_bench")
    set_kind("binary")
    add_rules("gamegine.warnings", "gamegine.werror")

    -- Kernels are compiled ahead of time, never at run time.
    add_deps("shaders")

    -- Its defaults, build/shaders and results/, are relative to the root.
    set_rundir("$(projectdir)")

    add_includedirs("bench")
    add_files("bench/vg_bench.cpp", "bench/bench_vulkan.cpp")
    -- meshoptimizer is page_decode's provisional codec.
    add_packages("vulkansdk", "meshoptimizer", "nlohmann_json", "picosha2")

    if is_plat("macosx") then
        -- The Metal backend, in C++ through metal-cpp.
        add_files("bench/bench_metal.cpp")
        add_packages("metal-cpp")

        -- The same loader rpath the engine library needs; see there.
        local vulkan_sdk = os.getenv("VULKAN_SDK")
        if vulkan_sdk then
            add_rpathdirs(path.join(vulkan_sdk, "lib"))
        end
        add_rpathdirs("/usr/local/lib")
    elseif is_plat("linux") then
        add_syslinks("pthread", "dl", "m")
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
    before_build(function (_)
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
