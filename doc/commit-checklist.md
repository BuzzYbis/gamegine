# Before committing

What to run and check before a commit leaves your machine. The first section
is for every commit; the others apply when the commit touches what they
name. Every command runs from the repository root. The tasks themselves are
described in [ci/README.md](../ci/README.md).

## Every commit

- [ ] **Format.** `xmake format` (clang-format, then the BDE pass). Your
      editor's clang-format undoes the BDE banners, so formatting on save is
      not enough.
- [ ] **Push tier.** `xmake ci-check` passes: format, Lua, the CI tooling's
      tests, the unit tests, the pins. It is what the runner runs, in the
      runner's order.
- [ ] **Static analysis.** `xmake ci-tidy` reports every translation unit
      clean. It is not in the push tier, so nothing else will catch it. It
      reads `compile_commands.json`, which a build refreshes: build first if
      you added a file.
- [ ] **What goes in.** `git status`: no build output, no scratch files, no
      half-written component you did not mean to commit. Build output and
      `compile_commands.json` are ignored by git already.
- [ ] **Branch.** Not `main`. One commit per change a reviewer can read on
      its own.

## C++ under `gamegine/` or `tests/`

- [ ] **Sanitizers.** The unit tests pass under ASan and UBSan:
      `xmake f -m asan && xmake ci-unit`, then switch back to your usual
      mode. A death test that checks an `assert` (`EXPECT_DEBUG_DEATH`) only
      proves something in a build without `NDEBUG`.
- [ ] **GCC.** Anything with templates, `constexpr`, `consteval` or a
      `switch` over an enumeration compiles with GCC, the Profile L compiler.
      It rejects code clang accepts, such as a missing return after an
      exhaustive `switch`:
      ```sh
      printf '#include "engine/core/<component>.h"\n' |
          g++-16 -std=c++23 -fno-exceptions -Wall -Wextra -Wpedantic -Wshadow \
                 -Wconversion -Wsign-conversion -Werror \
                 -Igamegine/include -x c++ -fsyntax-only -
      ```
- [ ] **No exceptions.** Nothing throws or catches (D9); a fallible call
      returns a `Result` or a `Status`.

## A new or changed component

- [ ] **Tests.** The component has its `<component>.t.cpp`, testing the
      edges: buffer ends, values no enumerator names, sentinels. A fixed bug
      gets a test that fails without the fix.
- [ ] **Header documentation**, as in
      [CodingStandards(fromBDE_almost).pdf](CodingStandards(fromBDE_almost).pdf):
      `@PURPOSE` (one imperative line), `@CLASSES`, `@SEE_ALSO`,
      `@DESCRIPTION`, and a `///Usage` section with an example.
- [ ] **Contracts.** Every function: "the specified [x]", "Optionally
      specify", "The behavior is undefined unless", "Return". Code in
      comments is marked `[like_this]`. Comments short.
- [ ] **Names.** Constants `k_UPPER_CASE`, members `d_`, template
      parameters `t_`, enumerators `e_`. A named constant over a bare number,
      and an enumeration over a `bool` argument.
- [ ] **Tables.** Every table indexed by an enumeration has a
      `static_assert` on its size against `e_COUNT`.
- [ ] **Layout.** Inline definitions under the INLINE DEFINITIONS banner. An
      exception (a function a constant expression needs earlier) says why in
      a comment.
- [ ] **Related files.** A component other headers refer to is in their
      `@SEE_ALSO`.

## A claim about speed

- [ ] **Measured, not guessed.** Any "faster" in a comment or a commit
      message comes from a benchmark at `-O3 -DNDEBUG`, inputs hidden from
      the optimiser, best of 9, with clang and GCC. The comment gives the
      number and says what was compared.
- [ ] **Same output.** The faster version was checked against the old one
      on the same inputs before the old one was removed.

## Documentation

- [ ] A decision that changed is updated where it is recorded
      ([architecture.md](architecture.md), [roadmap.md](roadmap.md),
      [ci.md](ci.md)), not only in code comments.
