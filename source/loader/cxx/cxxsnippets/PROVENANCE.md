The cxxsnippets compiler, preprocessor, type system, machine-code emitter and
runtime were written from scratch for this project. No TinyCC, chibicc or
GPL/LGPL compiler source was used or consulted.

External submodules keep their own licenses. Hooking.Patterns is used through
its public interface; its source and license are in `external/Hooking.Patterns`.
Injector, safetyhook and Zydis keep their notices in `external/injector`. These
are runtime dependencies, not sources for the compiler.
