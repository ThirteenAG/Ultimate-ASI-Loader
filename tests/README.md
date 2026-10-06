# Ultimate ASI Loader tests

Everything the tests need is **built from source** by `tests/premake5.lua`. The repository no longer contains prebuilt sample executables, DLLs or fixture files. Test fixtures (game directories, ini files, zip packages, split archives) are generated at run time in a throw-away sandbox under `%TEMP%\ual-tests`.

The test binaries:

| Binary | What it tests | How |
|---|---|---|
| `bin\<arch>\Release\tests\ual_unit_tests.exe` (Win32 and x64) | The loader modules: string and ini helpers, `OverloadFromFolder` parsing and priorities, path keys, zip archives and packages (multi-part, Unicode paths, concurrent extraction), update folder / zip layers (priorities, listings, the folder watcher, the path exports), proxy ordinal tables, IAT and ordinal patching (on synthetic PE images) | Compiled with the loader sources (all of `source/loader` except `main.cpp`) and calls them directly. No API is hooked. |
| `bin\x64\Release\tests\ual_tests.exe` | The real `dinput8.dll` of both architectures, end to end | For each test it creates a game directory, deploys the loader under a proxy name, a test "game" executable and probe plugins, runs the game, and checks the event log. Dialogs are read through UI Automation and clicked automatically. |
| `bin\<arch>\Release\tests\ual_cxxsnippets_tests.exe` (Win32 and x64) | The `.cxx` snippet engine (`source/loader/cxx/cxxsnippets`): language features, ABI, preprocessor (UTF-8 paths), source locations, `Check`, `Unload` (hooks released, memory writes reverted), crashes caught and located, game-style hooks (a `__thiscall` method replaced, a DLL export hooked, Windows API calls), 5,000 results compared with MSVC, and every `examples/snippets/*.cxx` compiled (also built natively by the `snippet-examples-native` project) | Unit checks in-process; the use cases in `tests/cxxsnippets/cases/*.cxx` run in child processes, both compiled by the engine and as native DLLs built by MSVC (`case-<name>.dll`), compared with `tests/cxxsnippets/golden`. |
| `bin\Win32\Release\tests\ual_wndmode_tests.exe` | The window mode compatibility layer (`source/compat/wndmode`) | In-process, against real windows. |

## Building

The test projects are part of the main solution (solution folder `tests`):

```bat
premake5 vs2026
msbuild -m build\Ultimate-ASI-Loader.slnx -p:Configuration=Release -p:Platform=x64
msbuild -m build\Ultimate-ASI-Loader.slnx -p:Configuration=Release -p:Platform=Win32
```

The test projects produce, in `bin\<arch>\Release\tests`:

| File | Purpose |
|---|---|
| `ual_host.exe` | Test "game" with no proxy import; loads proxies at run time (`load:<dll>`) |
| `ual_host_<proxy>.exe` | Test "game" that statically imports `<proxy>.dll`, so the loader is mapped before the entry point like in a real game (one per proxy with a Windows SDK import library) |
| `probe.asi` | Instrumented plugin; records `DllMain`/`InitializeASI` calls, loader-lock state, current directory and so on. Behaviour is selected by its file name: `*failinit*` makes `DllMain` fail, `*chdir*` changes the current directory |
| `probe_missingdep.asi` | Plugin importing `ual_missing_dependency.dll`, which is never deployed |
| `ual_fake_original.dll` | Stand-in for an original DLL (`<name>Hooked.dll` chaining) that returns sentinel values |
| `stubs\vorbis.dll` (Win32) | Stand-in for a game's `vorbis.dll`, used by the built-in vorbisfile of the 32-bit loader |
| `ual_unit_tests.exe`, `ual_tests.exe` | Test runners |

## Running

```bat
bin\x64\Release\tests\ual_unit_tests.exe
bin\Win32\Release\tests\ual_unit_tests.exe
bin\x64\Release\tests\ual_tests.exe
bin\x64\Release\tests\ual_cxxsnippets_tests.exe
bin\Win32\Release\tests\ual_cxxsnippets_tests.exe
bin\Win32\Release\tests\ual_wndmode_tests.exe
```

Common options for both runners:

| Option | Meaning |
|---|---|
| `--list` | List tests (respects the filters) |
| `--filter <text>` | Run tests whose name contains `<text>`, or matches a `*`/`?` wildcard (repeatable). A bare argument is a filter too, e.g. `ual_tests.exe "[x64]*zip*"` |
| `--tags a,b` / `--exclude-tags a,b` | Select by tag: `loading`, `proxy`, `overload`, `vfs`, `vpath`, `zip`, `plugins`, `dialog`, `ui`, `slow`, `stress`, `perf`, `Win32`, `x64`, ... |
| `--junit <file>` | Write a JUnit XML report |
| `--keep` | Keep every sandbox. Sandboxes of failing tests are always kept, and their path is printed |
| `--repeat <n>`, `--fail-fast`, `--verbose` | |
| `--bin <dir>`, `--config <name>` (`ual_tests.exe` only) | Where the build outputs are; the default is derived from the runner's own location |

When `GITHUB_STEP_SUMMARY` is set, a Markdown summary is appended to it.

> **Note:** `[ui]` tests show real dialogs and click them automatically. The loader stops the folder-selection countdown when the user touches the mouse or keyboard (by design). The two countdown tests therefore SKIP when any input happened during their run. Leave the machine alone, or pass `--exclude-tags ui`.


## Extending

- **New proxy name:** add a line to `tests/runner/proxies.hpp`. The static/dynamic loading tests and the export forwarding test are generated from that table. If a Windows SDK import library exists, also add it to `static_hosts` in `tests/premake5.lua`, and to `tests/host/host_imports.cpp`. To compare an export with the system DLL, add a branch to the `forward` scenario in `tests/host/scenarios_misc.cpp`.
- **New end-to-end test:** use `ARCH_TEST("name", "[tags]")` (or `WIN32_TEST` / `X64_TEST`) in a `tests/runner/tests_*.cpp` file. Build a `Sandbox`, deploy the files the scenario needs, and call `Run()` with host actions such as `read:<path>`, `attrex:<path>`, `find:<pattern>`, `loadlib:<path>`, `vadd:<path>|<data>|<prio>`, `vpath:<orig>|<target>|<prio>`, `ovpath`, `ovfile:<path>`, `trigger:<api>` or `snapshot`. The full list is at the top of `tests/host/host_main.cpp`. Then assert on the returned records.
- **New in-process scenario:** add `HOST_SCENARIO(name) { host.Check(cond, "what"); }` to `tests/host/scenarios_*.cpp`, and run it with the `scenario:name[|args]` action plus `REQUIRE_SCENARIO(r, "name")`. Where Win32 semantics are subtle, compare the virtual file against a real file in the same scenario, as the existing `vf_*` scenarios do.
- **New Windows exports:** `tests_exports.cpp` fails when a system DLL exports a name the loader does not. Add the function to `source/loader/x86.def` / `x64.def` and to `source/loader/proxy/exports.inl` / `thunks.inl`. If games never import it, add it to `kKnownExportGaps` with a reason instead.
- **New unit test:** add a `TEST_CASE` to `tests/unit/unit_tests.cpp`. The headers of `source/loader` are available; the internal interfaces meant for tests are `vfs/internal.hpp`, `vfs/packages.hpp` and `startup/imports.hpp`. Update folder layers can be activated only once per process.
