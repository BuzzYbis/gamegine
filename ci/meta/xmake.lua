---@meta
-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- Type definitions for the parts of xmake's Lua sandbox this project uses.
--
-- xmake extends the standard 'os', 'io', 'table' and 'string' libraries and
-- adds globals of its own. Without these declarations the language server
-- reports 140 undefined fields across ci/, which buries the findings that
-- matter. Silencing 'undefined-field' wholesale would have hidden them too --
-- and would have cost the completion and type checking these bring.
--
-- Only what the project actually calls is declared. An entry here is a claim
-- that the API exists and behaves as described; adding one speculatively
-- would make this file a source of wrong answers.
--
-- Referenced from .luarc.json via workspace.library.

--#region os

---Absolute path of the project root (the directory holding xmake.lua).
---@return string
function os.projectdir() end

---Absolute path of the directory holding the running script.
---@return string
function os.scriptdir() end

---Host operating system: "macosx", "linux", "windows", ...
---@return string
function os.host() end

---Host architecture: "arm64", "x86_64", ...
---@return string
function os.arch() end

---Monotonic millisecond clock, for measuring elapsed time.
---@return number
function os.mclock() end

---True if the path exists and is a regular file.
---@param filepath string
---@return boolean
function os.isfile(filepath) end

---True if the path exists and is a directory.
---@param dirpath string
---@return boolean
function os.isdir(dirpath) end

---Expand a glob pattern to matching files. '**' recurses.
---@param pattern string
---@return string[]
function os.files(pattern) end

---Expand a glob pattern to matching directories.
---@param pattern string
---@return string[]
function os.dirs(pattern) end

---Create a directory, including missing parents.
---@param dirpath string
function os.mkdir(dirpath) end

---Move a file or directory.
---@param src string
---@param dst string
function os.mv(src, dst) end

---Remove a file or directory; raises if it does not exist.
---@param filepath string
function os.rm(filepath) end

---Remove a file or directory, ignoring failure.
---@param filepath string
---@return boolean
function os.tryrm(filepath) end

---Path of the system temporary directory.
---@return string
function os.tmpdir() end

---Path of a fresh temporary file.
---@return string
function os.tmpfile() end

---Run a program with an argument vector.
---
---With `opt.try` it returns the exit code instead of raising, and nil when
---the program could not be started at all. With `opt.timeout` (milliseconds)
---it kills the child and returns -1. Both matter here: every probe must be
---bounded and must never raise.
---@param program string
---@param argv? string[]
---@param opt? {stdout?: string, stderr?: string, try?: boolean, timeout?: number, envs?: table, curdir?: string}
---@return integer|nil
function os.execv(program, argv, opt) end

---Run a program verbosely, raising on failure.
---@param program string
---@param argv? string[]
---@param opt? table
function os.vrunv(program, argv, opt) end

--#endregion

--#region io

---Read an entire file. Returns nil if it cannot be read.
---@param filepath string
---@return string|nil
function io.readfile(filepath) end

---Write a string to a file, replacing any existing contents.
---@param filepath string
---@param content string
function io.writefile(filepath, content) end

--#endregion

--#region table

---Shallow copy of a table.
---@generic T: table
---@param tbl T
---@return T
function table.clone(tbl) end

--#endregion

--#region string

---Remove leading and trailing whitespace. Available as a method: `s:trim()`.
---@param s string
---@return string
function string.trim(s) end

---True if the string starts with the given prefix. Method: `s:startswith(p)`.
---@param s string
---@param prefix string
---@return boolean
function string.startswith(s, prefix) end

---True if the string ends with the given suffix. Method: `s:endswith(x)`.
---@param s string
---@param suffix string
---@return boolean
function string.endswith(s, suffix) end

---Split a string on a separator pattern. Method: `s:split(sep)`.
---@param s string
---@param sep string
---@param opt? table
---@return string[]
function string.split(s, sep, opt) end

--#endregion

--#region path

path = {}

---Join path components.
---@param ... string
---@return string
function path.join(...) end

---The directory part of a path.
---@param filepath string
---@return string
function path.directory(filepath) end

---The final component of a path.
---@param filepath string
---@return string
function path.filename(filepath) end

---The final component without its extension.
---@param filepath string
---@return string
function path.basename(filepath) end

---Express one path relative to another.
---@param filepath string
---@param rootdir? string
---@return string
function path.relative(filepath, rootdir) end

---True if the path is absolute.
---@param filepath string
---@return boolean
function path.is_absolute(filepath) end

--#endregion

--#region hash

hash = {}

---SHA-256 of a file's contents, as lowercase hex.
---@param filepath string
---@return string
function hash.sha256(filepath) end

---MD5 of a file's contents.
---@param filepath string
---@return string
function hash.md5(filepath) end

--#endregion

--#region sandbox globals

---Load a module. Returns its table and also binds it as a global of the
---module's short name, which is why imported names appear "undefined".
---@param name string
---@param opt? {rootdir?: string, alias?: string, try?: boolean}
---@return table
function import(name, opt) end

---Structured error handling. The sandbox provides no 'pcall'.
---@param block table
function try(block) end

---@param block table
---@return table
function catch(block) end

---@param block table
---@return table
function finally(block) end

---Raise an error with a formatted message.
---@param fmt string
---@param ... any
function raise(fmt, ...) end

---Print with xmake colour markup, e.g. "${color.success}done".
---@param fmt string
---@param ... any
function cprint(fmt, ...) end

---@param fmt string
---@param ... any
function cprintf(fmt, ...) end

---Format a string with xmake's variable expansion.
---@param fmt string
---@param ... any
---@return string
function format(fmt, ...) end

--#endregion

--#region project and task definition

-- Like 'option', 'task' is both a declaration form and, after
-- import("core.base.task"), a module that can invoke another task.

---@class XmakeTask
---@overload fun(name: string)
task = {}

---Run another task by name, passing its options as a table.
---@param name string
---@param options? table
function task.run(name, options) end

function task_end() end
---@param menu table
function set_menu(menu) end
---@param category string
function set_category(category) end
---@param callback function
function on_run(callback) end

---@param name string
function target(name) end
function target_end() end
---@param name string
function rule(name) end
function rule_end() end
-- 'option' wears two hats. In xmake.lua it declares a build option:
--     option("asan")
-- and inside a task, after import("core.base.option"), it is the module that
-- reads the parsed command line:
--     option.get("out")
-- The @overload makes both shapes type-check.

---@class XmakeOption
---@overload fun(name: string)
option = {}

---Value of a command-line option declared in the task's set_menu.
---
---The return type depends on the option's declared kind -- "kv" yields a
---string, "k" a boolean, "vs" a table -- and that declaration is not visible
---from here, so the honest annotation is `any`. Narrowing it to `string`
---would be a lie that also suppressed the nil checks every caller needs,
---since an option the user did not pass returns nil.
---@param name string
---@return any
function option.get(name) end

function option_end() end
---@param show boolean
function set_showmenu(show) end

---@param ... string
function add_files(...) end
---@param ... string
function remove_files(...) end
---@param ... string
function add_headerfiles(...) end
---@param kind string
function set_kind(kind) end
---@param ... string
function includes(...) end

--#endregion
