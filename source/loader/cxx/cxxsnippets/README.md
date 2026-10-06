# cxxsnippets

A standalone Windows library that compiles small C++ snippets to native x86 or
x64 code inside the calling process. The host links a static library; running a
snippet needs no compiler, SDK, subprocess or temporary DLL. The compiler and
runtime are MIT licensed and written from scratch. See
[PROVENANCE.md](PROVENANCE.md) and the licenses in the external submodules.

This file, for example, is tested both through cxxsnippets and as an MSVC-built
DLL:

```cpp
#include <injector/injector.hpp>
#include <Hooking.Patterns.h>

void Init() {
    injector::WriteMemory<uint8_t>(
        hook::get_pattern("74 10 53 53 6A 1B"), 0xEB, true);
}
```

It is the engine behind Ultimate ASI Loader's `.cxx` snippets. The loader finds
the files, reports problems and reloads them (`source/loader/cxx` and the
repository readme); this folder only compiles and runs them. It was developed as
the separate tinycpp project and moved here.

## Build and test

Built as part of Ultimate ASI Loader (project `cxxsnippets`, a static library
with safetyhook, Zydis and Hooking.Patterns). The tests are
`ual_cxxsnippets_tests.exe` (`tests/cxxsnippets`): unit checks, a differential
comparison with MSVC, and the use cases in `tests/cxxsnippets/cases/*.cxx`, each
run through the engine and as an MSVC-built DLL (`case-<name>.dll`).

## Host API

```cpp
#include "cxxsnippets.hpp"

std::vector<cxxsnippets::Diagnostic> errors;
cxxsnippets::Options options;
options.symbols.push_back({"HostFunction", reinterpret_cast<void*>(HostFunction)});
auto module = cxxsnippets::CompileFile("patch.cpp", options, errors);
if (!module) {
    for (const auto& error : errors)
        Report(error.ToString());
    return;
}
module->RunInit();
// Keep module alive while its functions, callbacks, and globals are used.
```

`Compile(source, fileName, options, errors)` compiles in-memory text; the file
name is used for diagnostics and relative includes. A successful compile runs
global initialization. `Init` and `Shutdown`, if present, must be unambiguous
`void()` functions. `RunInit()` runs `Init` once and returns false if it is
missing or the module was shut down. `RunShutdown()` runs `Shutdown` once.
Destruction also runs `Shutdown`, releases global hook handles in reverse
declaration order, releases pattern allocations, removes unwind registrations
and frees code and data memory.

Paths (`CompileFile`, include directories, diagnostics) are UTF-8.

`Check(source, fileName, options, errors)` and `CheckFile` compile without
loading or running anything. Hot reload uses them to validate an edit before
touching the running version.

A crash (structured exception) in snippet code is caught where the host asks
for it: global initialization during `Compile` (reported as an error at the
crashing line, with the snippet's effects undone), and `RunInit(&crash)` and
`RunShutdown(&crash)`, where `Crash` holds the exception, address and source
location. `Contains(address)` and `Locate(address, location)` map an address in
a snippet's code to its file, line and function (per statement) for crash
reports.

`Unload()` stops a snippet without freeing it. It runs `Shutdown`, releases its
hooks and owned objects, and restores every byte changed by its memory writes
(`WriteMemory`, `WriteMemoryRaw`, `MakeNOP`, `MakeJMP`, `MakeCALL`), newest
first, each location to its oldest bytes. The code stays mapped so calls still
running on other threads can finish; it is freed with the `Module`. Direct
stores through pointers are not tracked.

`Find("name")` returns the address of an externally visible function or
variable, qualified names such as `patch::counter` included. Overloaded names
are ambiguous and return null. Cast a function address to its exact signature
and calling convention. `CodeSize()` is the machine code size; `DataSize()`
includes globals and string literals.

Symbols resolve in this order: built-in functions, `Options.symbols`,
`Options.resolve`, then exports of DLLs already loaded in the process. A used
symbol that doesn't resolve is a compile error. `Options.defines`,
`includeDirs` and `headers` supply command-line defines, include directories
and in-memory headers. In-memory header names ignore case and slash direction
and can override supplied headers.

Quoted includes search the including file's directory, then in-memory headers,
then `includeDirs`. Angle includes search in-memory headers and `includeDirs`.
File system include paths are canonicalized for `#pragma once`.

## Supported language

The subset covers:

- Functions, globals, automatic and static locals; `extern "C"`, `static`,
  `inline`, `const` and constant scalar `constexpr` declarations.
- Windows LLP64 fundamental types, pointers, references, multidimensional
  arrays, function-pointer declarators, `auto`, `nullptr`, typedefs, aliases,
  namespaces (including reopened ones and namespace imports), enums and scoped
  enums.
- Structs, classes and unions with fields, aggregate initialization and
  copying, default member initializers and inline nonvirtual member functions.
  Member bodies are parsed after the class is complete.
- Arithmetic, bitwise, comparison, logical, assignment, compound assignment,
  increment, dereference, address-of, subscript, conditional and comma
  expressions; `sizeof`, C-style and functional casts, and the three named
  casts.
- `if`, `for`, `while`, `do`, `switch` with fallthrough, `break`, `continue`,
  labels, `goto` and `return`. Compound assignment evaluates its address once.
- `static_assert` at namespace, class and block scope, and `offsetof` with
  nested members and constant subscripts.
- Captureless lambdas with deduced return types, convertible to compatible
  cdecl, stdcall, fastcall or thiscall function pointers.
- Overload ranking by exact match, promotion, conversion, built-in conversion
  and ellipsis, plus default arguments. Bodyless template bindings in supplied
  headers support explicit and deduced type arguments, defaults and type packs.
- Narrow, UTF-8 and Windows UTF-16 wide literals, escapes, raw strings and
  adjacent strings of the same width. Variadic calls apply default promotions.
- Includes, object and function macros, stringification, token pasting,
  variadic macros, conditional directives, `defined`, `__has_include` and
  `#pragma once`; Windows/MSVC architecture macros and `__CXXSNIPPETS__`.

Not supported: user templates, virtual functions, inheritance, user
constructors and destructors, operator overloading, exceptions, lambda captures
and the C++ standard library. The small built-in `cstdint`, `cstddef`, `cstdio`
and `cstring` headers provide selected types and C functions only.

Also missing for now: passing or returning aggregates by value (use references
or pointers), out-of-class member definitions, bitfields, `new`/`delete`
expressions, generic lambdas, mixed-width adjacent strings, lifetime extension
of temporaries bound to references, and packing pragmas. Scalar `constexpr`
functions can be evaluated in constant expressions, recursion and control flow
included, within fixed recursion and step limits. Aggregate and pointer
operations are not supported in constant evaluation. Static local
initialization is serialized; recursive initialization and recovering from an
exception during initialization are not supported. This is a bounded snippet
compiler, not an ISO C++ implementation.

## Built-in bindings and lifetime

`BuiltinHeaders()` lists the available headers. Besides the C/Windows subset:

- `injector/injector.hpp` binds typed and raw memory reads and writes, NOP, JMP
  and CALL to the real injector library. Addresses can be integers or
  pointers; writes honor the protection flag and flush the instruction cache.
  On x64, `MakeJMP`/`MakeCALL` route a destination outside rel32 range through
  a 14-byte jump stub allocated within 2 GB of the patched instruction and kept
  until the module is destroyed. `MakeJMPTrampoline` and `MakeCALLTrampoline`
  are aliases for them.
- `Hooking.Patterns.h` binds pattern constructors, counting, clearing,
  querying, typed result access, iteration, and process, module and range
  lookups to the real pattern library. Pattern and match values are
  pointer-sized; copies share an allocation owned by the compiled module.
- `safetyhook.hpp` provides context layouts matching the linked library and
  inline, mid, VMT and VM hook factories. Inline and mid hooks expose reset,
  enable, disable, enabled, target address and boolean state. Inline and VM
  hooks expose original function pointers and calls in each calling
  convention, unsafe forms included. VMT/VM methods follow the real library's
  API.

Hook handles must be globals or temporaries; automatic hook-handle variables
and handle copies are rejected. Assignment releases the previous handle and
`= {}` clears it. Temporary handles are released at the end of the full
expression, including temporaries used as method receivers. Stop threads the
snippet created and let callbacks finish before destroying a module: code and
data addresses it returned are only valid while it is alive.

To publish an inline hook safely across threads, create it with
`StartDisabled`, assign the global handle, then enable it, as the threadsafe
example does. Hooking and patching keep the semantics of the underlying
libraries, including their pattern count requirements.

## Execution and validation

The emitter produces native instructions plus calls to fixed arithmetic and
conversion helpers; nothing interprets the AST at run time. Expressions use a
stack of eight-byte values. Win32 supports cdecl, stdcall, fastcall, thiscall,
x87 floating-point returns and 64-bit helpers. x64 passes Windows
register/home/stack arguments with aligned calls and duplicates floating-point
varargs into integer registers. Internal calls use rel32 relocations. After
relocation code becomes read/execute; data stays read/write. x64 function
tables describe the prologues, large-frame stack probing included, with RBP as
the frame register: expressions push temporaries, so unwinding (exceptions,
crash reports, stack walks) goes through RBP instead of a fixed stack pointer.

Tests run compiled functions in-process, compare the same corpus against MSVC
over 5,000 input/result pairs, and run seven isolated use cases both through
the engine and as native DLLs against golden files: minimal, multiple,
threadsafe, midhook, vmthook, PeekMessage hooks and the pattern patch above.
The unit suite also covers temporary and global hook cleanup, code protection,
const diagnostics, calling conventions, side effects, large frames and x64
virtual unwinding.
