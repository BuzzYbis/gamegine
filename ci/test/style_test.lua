-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- Guards the text of the GitHub annotations the CI tasks print when a check
-- fails on a hosted runner, where the annotation is all that can be read
-- without a login: a message that broke the command would be lost, and with
-- it the reason for the failure.

import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
import("testing", {rootdir = path.join(os.projectdir(), "ci", "lua")})

function run(t)
    testing.equal(t, "a plain message is a workflow command",
                  style.annotation_text("error", "unit", "it failed"),
                  "::error title=unit::it failed")

    testing.equal(t, "a newline in the message is written %0A",
                  style.annotation_text("error", "unit", "a\nb"),
                  "::error title=unit::a%0Ab")

    testing.equal(t, "a percent sign is written first, so it is not doubled",
                  style.annotation_text("error", "unit", "100%\n"),
                  "::error title=unit::100%25%0A")

    testing.equal(t, "a colon and a comma in the title end a property",
                  style.annotation_text("warning", "a:b,c", "m"),
                  "::warning title=a%3Ab%2Cc::m")

    testing.equal(t, "a colon in the message is left alone",
                  style.annotation_text("error", "t", "a: b, c"),
                  "::error title=t::a: b, c")

    testing.equal(t, "a long message is cut and says so",
                  style.annotation_text("error", "t", ("x"):rep(20), 5),
                  "::error title=t::xxxxx [cut]")
end
