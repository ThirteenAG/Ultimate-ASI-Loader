#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include "cxxsnippets.hpp"
#include "preprocessor.hpp"
#include "parser.hpp"
#include "codegen.hpp"
#include "builtins.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <map>
#include <cstring>

namespace cxxsnippets
{
std::string Diagnostic::ToString() const
{
    return file + "(" + std::to_string(line) + "," + std::to_string(column) + "): error: " + message;
}
namespace
{
// Runs fn, catching structured exceptions (no C++ objects here: __try).
unsigned long CallGuarded(void (*fn)(), const void **faultAddress)
{
    __try
    {
        fn();
        return 0;
    }
    __except (*faultAddress = (GetExceptionInformation())->ExceptionRecord->ExceptionAddress, EXCEPTION_EXECUTE_HANDLER)
    {
        return GetExceptionCode();
    }
}

class LoadedModule final : public Module
{
  public:
    uint8_t *code = nullptr, *data = nullptr;
    size_t codeSize = 0, dataSize = 0;
    std::map<std::string, void *> exports;
    void (*init)() = nullptr;
    void (*shutdown)() = nullptr;
    bool initialized = false, started = false, initOk = false, stopped = false, unloaded = false;
    BuiltinOwner owner;
    // source information: functions (code range, name), statements (offset, file, line)
    struct FunctionRange
    {
        size_t begin, end;
        std::string name;
    };
    std::vector<FunctionRange> functions;
    std::vector<std::pair<size_t, Loc>> lines; // sorted by offset
    std::vector<std::string> files;
    std::vector<std::pair<void **, void (*)(void *)>> handles;
#ifdef _WIN64
    std::vector<RUNTIME_FUNCTION> unwind;
    bool registered = false;
#endif
    ~LoadedModule() override
    {
        Unload();
#ifdef _WIN64
        if (registered)
            RtlDeleteFunctionTable(unwind.data());
#endif
        if (code)
            VirtualFree(code, 0, MEM_RELEASE);
        if (data)
            VirtualFree(data, 0, MEM_RELEASE);
    }
    void *Find(const std::string &name) const override
    {
        auto it = exports.find(name);
        return it == exports.end() ? nullptr : it->second;
    }
    // calls fn; with `crash`, catches and describes a crash
    bool Call(void (*fn)(), Crash *crash)
    {
        if (!crash)
        {
            fn();
            return true;
        }
        const void *at = nullptr;
        *crash = {};
        if (unsigned long code = CallGuarded(fn, &at))
        {
            owner.ReleaseStaticLocks(); // a static local's initializer may have crashed while holding them
            crash->code = code;
            crash->address = at;
            Locate(at, crash->where);
            return false;
        }
        return true;
    }
    bool RunInit(Crash *crash) override
    {
        if (!init || stopped)
            return false;
        if (!started)
        {
            started = true;
            initOk = Call(init, crash);
            return initOk;
        }
        return true;
    }
    void RunShutdown(Crash *crash) override
    {
        if (!stopped)
        {
            stopped = true;
            if (shutdown)
                Call(shutdown, crash);
        }
    }
    // the statement that contains a code offset
    Loc LocOf(const void *address) const
    {
        if (!Contains(address))
            return {};
        size_t offset = (const uint8_t *)address - code;
        auto it = std::upper_bound(lines.begin(), lines.end(), offset, [](size_t o, const auto &e) { return o < e.first; });
        return it == lines.begin() ? Loc{} : std::prev(it)->second;
    }
    void Unload() override
    {
        if (unloaded)
            return;
        unloaded = true;
        // Shutdown undoes what Init did: skip it when Init never ran to completion (it crashed, or was never
        // called), since it would act on half-built state. Hooks and memory writes are reverted below anyway.
        if (initialized && (!init || initOk))
        {
            Crash crash; // a crash in Shutdown must not stop the cleanup
            RunShutdown(&crash);
        }
        stopped = true;
        for (auto i = handles.rbegin(); i != handles.rend(); ++i)
        {
            if (*i->first)
                i->second(*i->first);
            *i->first = nullptr;
        }
        owner.Release();
        owner.RevertWrites();
    }
    bool Unloaded() const override
    {
        return unloaded;
    }
    bool Contains(const void *address) const override
    {
        auto a = (const uint8_t *)address;
        return code && a >= code && a < code + codeSize;
    }
    bool Locate(const void *address, SourceLocation &out) const override
    {
        if (!Contains(address))
            return false;
        size_t offset = (const uint8_t *)address - code;
        out = {};
        for (auto &f : functions)
            if (offset >= f.begin && offset < f.end)
                out.function = f.name;
        // the last statement that starts at or before the address
        auto it = std::upper_bound(lines.begin(), lines.end(), offset, [](size_t o, const auto &e) { return o < e.first; });
        if (it == lines.begin())
            return !out.function.empty();
        --it;
        if (it->second.file >= 0 && it->second.file < (int)files.size())
            out.file = files[it->second.file];
        out.line = it->second.line;
        return true;
    }
    size_t CodeSize() const override
    {
        return codeSize;
    }
    size_t DataSize() const override
    {
        return dataSize;
    }
};
void *Resolve(const Options &options, const std::string &name)
{
    if (auto builtin = BuiltinSymbol(name))
        return builtin;
    for (auto &s : options.symbols)
        if (s.name == name)
            return s.address;
    if (options.resolve)
        if (auto address = options.resolve(name))
            return address;
    HMODULE modules[1024];
    DWORD bytes = 0;
    if (EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &bytes))
        for (size_t i = 0; i < std::min<size_t>(bytes / sizeof(HMODULE), 1024); ++i)
            if (auto proc = GetProcAddress(modules[i], name.c_str()))
                return (void *)proc;
    return nullptr;
}
std::unique_ptr<Module> Load(TranslationUnit &u, Image &image, const SourceManager &sm)
{
    auto m = std::make_unique<LoadedModule>();
    m->codeSize = image.code.size();
    m->dataSize = image.data.size();
    for (auto fn : u.functions)
        if (fn->body || fn == u.initializer)
            m->functions.push_back({fn->codeOffset, fn->codeEnd, fn == u.initializer ? std::string("(global initialization)") : fn->qualified});
    for (auto &l : image.lines)
        m->lines.push_back({l.offset, l.loc});
    std::stable_sort(m->lines.begin(), m->lines.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    for (size_t i = 0; i < sm.files.size(); ++i)
        m->files.push_back(sm.files[i]->name);
#ifdef _WIN64
    for (auto fn : u.functions)
        if (fn->body || fn == u.initializer)
        {
            while (image.code.size() % 4)
                image.code.push_back(0);
            size_t offset = image.code.size();
            // Prologue: push rbp (offset 1); mov rbp, rsp (offset 4); sub rsp, N. The body pushes
            // temporaries so RSP isn't fixed; unwinding (exceptions, crash reports) uses RBP as frame register.
            // Unwind codes, newest first: alloc, set frame register, push rbp.
            image.code.insert(image.code.end(), {1, fn->prologueSize, (uint8_t)(fn->stackSize ? 4 : 2), 0x05 /* RBP, offset 0 */});
            if (fn->stackSize)
            {
                if (fn->stackSize / 8 > 65535)
                    Error(fn->loc, "function stack frame exceeds supported unwind size");
                image.code.push_back(fn->allocationOffset);
                image.code.push_back(1); // UWOP_ALLOC_LARGE, size / 8 in the next slot
                image.code.push_back((uint8_t)(fn->stackSize / 8));
                image.code.push_back((uint8_t)(fn->stackSize / 8 >> 8));
            }
            image.code.insert(image.code.end(), {4, 0x03}); // UWOP_SET_FPREG after mov rbp, rsp
            image.code.insert(image.code.end(), {1, 0x50}); // UWOP_PUSH_NONVOL rbp
            m->unwind.push_back({(DWORD)fn->codeOffset, (DWORD)fn->codeEnd, (DWORD)offset});
        }
#endif
    m->code = (uint8_t *)VirtualAlloc(nullptr, image.code.size(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!m->code)
        Error({}, "cannot allocate executable image");
    m->data = (uint8_t *)VirtualAlloc(nullptr, std::max<size_t>(1, image.data.size()), MEM_RESERVE | MEM_COMMIT,
                                      PAGE_READWRITE);
    if (!m->data)
        Error({}, "cannot allocate module data");
    for (auto &r : image.relocations)
    {
        uintptr_t dest = r.owner      ? (uintptr_t)&m->owner
                         : r.function ? (uintptr_t)m->code + r.function->codeOffset
                         : r.variable ? (uintptr_t)m->data + r.variable->offset
                         : r.absolute ? (uintptr_t)r.absolute
                                      : (uintptr_t)m->data + r.dataOffset;
        if (r.relative)
        {
            int64_t delta = (int64_t)dest - (int64_t)((uintptr_t)m->code + r.offset + 4);
            if (delta < INT32_MIN || delta > INT32_MAX)
                Error({}, "relative call is out of range");
            int32_t v = (int32_t)delta;
            std::memcpy(image.code.data() + r.offset, &v, 4);
        }
        else
            std::memcpy(image.code.data() + r.offset, &dest, sizeof(dest));
    }
    std::memcpy(m->code, image.code.data(), image.code.size());
    if (!image.data.empty())
        std::memcpy(m->data, image.data.data(), image.data.size());
    DWORD previous;
    if (!VirtualProtect(m->code, image.code.size(), PAGE_EXECUTE_READ, &previous))
        Error({}, "cannot protect executable image");
    if (!FlushInstructionCache(GetCurrentProcess(), m->code, image.code.size()))
        Error({}, "cannot flush instruction cache");
#ifdef _WIN64
    if (!RtlAddFunctionTable(m->unwind.data(), (DWORD)m->unwind.size(), (DWORD64)m->code))
        Error({}, "cannot register unwind information");
    m->registered = true;
#endif
    for (auto fn : u.functions)
        if (fn->body && !fn->internal)
        {
            if (m->exports.count(fn->qualified))
                m->exports[fn->qualified] = nullptr;
            else
                m->exports[fn->qualified] = m->code + fn->codeOffset;
        }
    for (auto v : u.globals)
        if (!v->external && !v->internal)
            m->exports[v->qualified] = m->data + v->offset;
    for (auto v : u.globals)
        if (!v->external && v->type->IsStruct() && v->type->st->handle)
        {
            auto release = (void (*)(void *))BuiltinSymbol(v->type->st->release);
            if (!release)
                Error(v->loc, "unresolved handle release");
            m->handles.push_back({(void **)(m->data + v->offset), release});
        }
    for (auto name : {"Init", "Shutdown"})
    {
        auto it = u.root.functions.find(name);
        if (it == u.root.functions.end())
            continue;
        auto &fns = it->second;
        if (fns.size() != 1 || !fns[0]->type->sig.ret->IsVoid() || !fns[0]->params.empty())
            Error(fns[0]->loc, std::string(name) + " must have signature void " + name + "()");
    }
    m->init = (void (*)())m->Find("Init");
    m->shutdown = (void (*)())m->Find("Shutdown");
    const void *at = nullptr;
    if (unsigned long code = CallGuarded((void (*)())(m->code + u.initializer->codeOffset), &at))
    {
        char message[96];
        snprintf(message, sizeof(message), "crashed during global initialization (exception 0x%08lX)", code);
        Error(m->LocOf(at), message); // m is destroyed: whatever it did is undone
    }
    m->initialized = true;
    return m;
}
} // namespace
std::unique_ptr<Module> Compile(const std::string &source, const std::string &fileName, const Options &options,
                                std::vector<Diagnostic> &errors)
{
    errors.clear();
    SourceManager sm;
    try
    {
        std::string dir;
        auto slash = fileName.find_last_of("\\/");
        if (slash != std::string::npos)
            dir = fileName.substr(0, slash + 1);
        int file = sm.Add(fileName, dir, source, false);
        PreprocessorConfig cfg;
        cfg.defines = options.defines;
        cfg.includeDirs = options.includeDirs;
        cfg.headers = HeaderSources();
        for (auto &h : options.headers)
        {
            std::string name = h.first;
            for (auto &c : name)
            {
                if (c == '\\')
                    c = '/';
                c = (char)tolower((unsigned char)c);
            }
            cfg.headers[name] = h.second;
        }
        auto tokens = Preprocess(sm, file, cfg);
        TranslationUnit unit(cfg.target);
        Parse(unit, tokens);
        auto image = Generate(unit, [&](const std::string &name) { return Resolve(options, name); });
        return Load(unit, image, sm);
    }
    catch (const CompileError &e)
    {
        errors.push_back({e.loc.file >= 0 ? sm.Get(e.loc.file).name : fileName, e.loc.line, e.loc.col, e.message});
    }
    catch (const std::exception &e)
    {
        errors.push_back({fileName, 0, 0, e.what()});
    }
    return nullptr;
}
std::unique_ptr<Module> CompileFile(const std::string &path, const Options &options, std::vector<Diagnostic> &errors)
{
    std::string text;
    if (!ReadSourceFile(path, text))
    {
        errors = {{path, 0, 0, "cannot open source file"}};
        return nullptr;
    }
    return Compile(text, path, options, errors);
}
bool Check(const std::string &source, const std::string &fileName, const Options &options, std::vector<Diagnostic> &errors)
{
    errors.clear();
    SourceManager sm;
    try
    {
        std::string dir;
        auto slash = fileName.find_last_of("\\/");
        if (slash != std::string::npos)
            dir = fileName.substr(0, slash + 1);
        int file = sm.Add(fileName, dir, source, false);
        PreprocessorConfig cfg;
        cfg.defines = options.defines;
        cfg.includeDirs = options.includeDirs;
        cfg.headers = HeaderSources();
        for (auto &h : options.headers)
        {
            std::string name = h.first;
            for (auto &c : name)
            {
                if (c == '\\')
                    c = '/';
                c = (char)tolower((unsigned char)c);
            }
            cfg.headers[name] = h.second;
        }
        auto tokens = Preprocess(sm, file, cfg);
        TranslationUnit unit(cfg.target);
        Parse(unit, tokens);
        Generate(unit, [&](const std::string &name) { return Resolve(options, name); });
        return true;
    }
    catch (const CompileError &e)
    {
        errors.push_back({e.loc.file >= 0 ? sm.Get(e.loc.file).name : fileName, e.loc.line, e.loc.col, e.message});
    }
    catch (const std::exception &e)
    {
        errors.push_back({fileName, 0, 0, e.what()});
    }
    return false;
}
bool CheckFile(const std::string &path, const Options &options, std::vector<Diagnostic> &errors)
{
    std::string text;
    if (!ReadSourceFile(path, text))
    {
        errors = {{path, 0, 0, "cannot open source file"}};
        return false;
    }
    return Check(text, path, options, errors);
}
std::vector<std::string> BuiltinHeaders()
{
    std::vector<std::string> result;
    for (auto &h : HeaderSources())
        result.push_back(h.first);
    return result;
}
} // namespace cxxsnippets
