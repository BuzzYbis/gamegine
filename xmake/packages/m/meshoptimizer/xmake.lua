-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- meshoptimizer, pinned to an exact upstream commit rather than a release.
--
-- Why a commit and not a tag. clusterlod.h lives in the upstream demo/
-- directory and tracks master, not releases. At the pinned commit it calls
-- meshopt_SimplifyErrorClamped and meshopt_SimplifyPreserveFolds, and neither
-- symbol exists in v1.2, the latest release at the time of pinning. Pairing
-- clusterlod.h with a release it predates does not link. The two are pinned
-- to the SAME commit and are bumped together, which is also what ci.md
-- section 6 requires: a bump of either is a content format change -- full
-- re-cook, new package hashes, named new baseline.
--
-- Verified on 2026-09-15: all 21 translation units plus a clusterlod.h
-- implementation unit compile and link under -std=c++23.

package("meshoptimizer")
    set_homepage("https://github.com/zeux/meshoptimizer")
    set_description("Mesh optimization library: clusterization, "
                    .. "simplification, codecs (D0)")
    set_license("MIT")

    add_urls("https://github.com/zeux/meshoptimizer/archive/$(version).tar.gz")

    -- Keep this commit and its hash in step with ci/pins.json; 'xmake ci-pins'
    -- fails if they drift apart.
    add_versions("7d8eca58818927c6b2dbcdd44763f95b798d4232",
                 "148ae3c53f8f007979ce74f43447b8e8f3bc4d04f934ce78f18bbb419aeecaa9")

    on_install(function (package)
        local configs = {}
        configs.kind = package:config("shared") and "shared" or "static"
        io.writefile("xmake.lua", [[
            set_languages("cxx23")
            target("meshoptimizer")
                set_kind("$(kind)")
                add_files("src/*.cpp")
                add_headerfiles("src/meshoptimizer.h")
        ]])
        import("package.tools.xmake").install(package, configs)
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            #include <meshoptimizer.h>
            void test() {
                // The two symbols absent from the v1.2 release. If this
                // compiles, the pin is the one clusterlod.h needs.
                unsigned int options = meshopt_SimplifyErrorClamped
                                     | meshopt_SimplifyPreserveFolds;
                (void)options;
            }
        ]]}, {configs = {languages = "c++23"}}))
    end)
