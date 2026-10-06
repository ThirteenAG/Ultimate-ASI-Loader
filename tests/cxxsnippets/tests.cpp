#include "cxxsnippets.hpp"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <crtdbg.h>

static int checks = 0;
static void Check(bool condition, const char *label)
{
    ++checks;
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", label);
        std::exit(1);
    }
}
static std::unique_ptr<cxxsnippets::Module> Build(const std::string &source, cxxsnippets::Options options = {})
{
    std::vector<cxxsnippets::Diagnostic> errors;
    auto module = cxxsnippets::Compile(source, "test.cpp", options, errors);
    for (auto &e : errors)
        std::fprintf(stderr, "%s\n", e.ToString().c_str());
    Check(module != nullptr, "compile");
    return module;
}
template <class F> static F Get(cxxsnippets::Module &m, const char *name)
{
    auto address = m.Find(name);
    Check(address != nullptr, name);
    return reinterpret_cast<F>(address);
}
static int hostAdd(int a, int b)
{
    return a + b;
}
static int __stdcall hostStdcall(int a, int b)
{
    return a * 3 + b;
}
static int __fastcall hostFastcall(int a, int b, int c)
{
    return a + b * 2 + c * 3;
}
static double hostDouble(double a, double b, double c, double d, double e)
{
    return a + b * 2 + c * 3 + d * 4 + e * 5;
}
static void Basic()
{
    auto m = Build(R"(
        int counter=3;
        int add(int a,int b){return a+b;}
        int fib(int n){if(n<2)return n;return fib(n-1)+fib(n-2);}
        int loop(int n){int total=0;for(int i=0;i<n;++i){if(i==3)continue;total+=i;if(i==8)break;}return total;}
        int control(int x){int n=0;do{++n;}while(n<x);while(n<10)++n;switch(x){case 1:return 11;case 2:n+=20;break;default:n+=30;}return n;}
        void Init(){counter+=4;} void Shutdown(){counter=0;}
    )");
    Check(Get<int (*)(int, int)>(*m, "add")(13, 29) == 42, "arithmetic");
    Check(Get<int (*)(int)>(*m, "fib")(10) == 55, "recursion");
    Check(Get<int (*)(int)>(*m, "loop")(20) == 33, "for break continue");
    auto control = Get<int (*)(int)>(*m, "control");
    if (control(1) != 11 || control(2) != 30 || control(3) != 40)
        std::fprintf(stderr, "control: %d %d %d\n", control(1), control(2), control(3));
    Check(control(1) == 11 && control(2) == 30 && control(3) == 40, "switch do while");
    Check(*(int *)m->Find("counter") == 3, "global initialization");
    Check(m->RunInit(), "Init exists");
    Check(*(int *)m->Find("counter") == 7, "Init runs");
    m->RunShutdown();
    Check(*(int *)m->Find("counter") == 0, "Shutdown runs");
}
static void Features()
{
    auto m = Build(R"(
        #include <cstdint>
        #include <cstddef>
        namespace test { typedef int I; enum class Kind { A=4,B }; int value=8; }
        namespace test { int get(){return value;} }
        using namespace test; using Alias=uint64_t;
        int array(int x){int values[]={3,5,7};int* p=values;return p[x]+sizeof(values);}
        int references(int x){int& r=x;r+=6;return x;}
        int flags(){return static_cast<int>(Kind::B)+get();}
        int overload(int x){return x+10;} int overload(double x){return 90;}
        int select(){return overload(2)+overload(2.0);}
        int defaults(int x,int y=4){return x*y;} int use_default(){return defaults(3);}
        struct Pair { int x=3; int y=4; int sum(){return x+y;} };
        int record(){Pair p{};p.x=8;return p.sum()+sizeof(Pair);}
        int lambda(){auto fn=[](int a,int b){return a*b;};return fn(6,7);}
        long long wide(long long x,long long y){return (x*y+9)/(y+1);}
        unsigned int wrap(unsigned int x){return x+1;}
        int shortcircuit(){int x=0;false&&(x=3);true||(x=4);return x;}
        int text(){char text[]="hello" "!";return text[1]+sizeof(text);}
    )");
    Check(Get<int (*)(int)>(*m, "array")(2) == 19, "arrays sizeof");
    Check(Get<int (*)(int)>(*m, "references")(5) == 11, "references");
    Check(Get<int (*)()>(*m, "flags")() == 13, "namespaces enums casts");
    Check(Get<int (*)()>(*m, "select")() == 102, "overloads");
    Check(Get<int (*)()>(*m, "use_default")() == 12, "default arguments");
    Check(Get<int (*)()>(*m, "record")() == 20, "struct member default initialization");
    Check(Get<int (*)()>(*m, "lambda")() == 42, "captureless lambda");
    Check(Get<long long (*)(long long, long long)>(*m, "wide")(10000000000LL, 5) == 8333333334LL, "64 bit arithmetic");
    Check(Get<unsigned (*)(unsigned)>(*m, "wrap")(0xffffffff) == 0, "unsigned wrap");
    Check(Get<int (*)()>(*m, "shortcircuit")() == 0, "short circuit");
    Check(Get<int (*)()>(*m, "text")() == 108, "adjacent strings");
}
static void ABI()
{
    cxxsnippets::Options options;
    options.symbols = {{"hostAdd", (void *)hostAdd},
                       {"hostStdcall", (void *)hostStdcall},
                       {"hostFastcall", (void *)hostFastcall},
                       {"hostDouble", (void *)hostDouble}};
    auto m = Build(R"(
        extern "C" int hostAdd(int a,int b); int __stdcall hostStdcall(int a,int b); int __fastcall hostFastcall(int a,int b,int c);
        double hostDouble(double a,double b,double c,double d,double e);
        int calls(){return hostAdd(10,20)+hostStdcall(3,4)+hostFastcall(1,2,3);}
        int __stdcall sc(int a,int b){return a-b;} int __fastcall fc(int a,int b,int c){return a+b+c;}
        int pointers(){int (__stdcall *f)(int,int)=sc;return f(20,3);}
        double floating(double x){return hostDouble(x,2.0,3.0,4.0,5.0)+0.5;}
        float single(float x){return x*2.5f;}
    )",
                   options);
    Check(Get<int (*)()>(*m, "calls")() == 57, "host calling conventions");
    Check(Get<int(__stdcall *)(int, int)>(*m, "sc")(20, 3) == 17, "stdcall definition");
    Check(Get<int(__fastcall *)(int, int, int)>(*m, "fc")(1, 2, 3) == 6, "fastcall definition");
    Check(Get<int (*)()>(*m, "pointers")() == 17, "function pointer declarator");
    Check(std::abs(Get<double (*)(double)>(*m, "floating")(1.0) - 55.5) < 0.00001, "floating host ABI");
    Check(Get<float (*)(float)>(*m, "single")(4.0f) == 10.0f, "float ABI");
}
static void Preprocessor()
{
    std::vector<cxxsnippets::Diagnostic> errors;
    auto disk = cxxsnippets::Compile(R"(
        #if !__has_include("include/probe.h")
        #error relative include probe failed
        #endif
        #if __has_include("include")
        #error directories are not headers
        #endif
        #include "include/probe.h"
        #include "include/../include/probe.h"
        int probe(){return filesystem_probe;}
    )",
                              std::string(CXXSNIPPETS_TEST_ROOT) + "/fixture.cpp", {}, errors);
    for (auto &e : errors)
        std::fprintf(stderr, "%s\n", e.ToString().c_str());
    Check(disk != nullptr, "filesystem include probe");
    Check(Get<int (*)()>(*disk, "probe")() == 7, "canonical pragma once");
    cxxsnippets::Options o;
    o.headers = {{"first.h", "#pragma once\n#include <nested.h>\nint a=3;\n"},
                 {"nested.h", "#pragma once\nint b=5;\n"},
                 {"other.h", "#pragma once\nint c=7;\n"}};
    auto m = Build(R"(
        #include <first.h>
        #include <other.h>
        #include <first.h>
        #include <nested.h>
        #define CAT(a,b) a##b
        #define TWICE(x) ((x)+(x))
        #if defined(_WIN32) && __has_include(<first.h>)
        int CAT(te,st)(){return TWICE(a)+b+c;}
        #else
        #error missing Windows definitions
        #endif
    )",
                   o);
    Check(Get<int (*)()>(*m, "test")() == 18, "preprocessing includes macros");
}
static void Errors()
{
    for (const char *source : {"int test(){return missing;}", "template<class T> T f(T x){return x;}",
                               "int test(){int x=3;auto f=[x](){return x;};return f();}",
                               "struct A { virtual int f(); };", "void Init(int x){}", "int test(){break;}",
                               // silently dropping an anonymous union would mislay every following member
                               "struct V { union { float x; float r; }; float y; };",
                               // a member function named in a class-scope expression before any function body was
                               // parsed: a diagnostic, not a crash (no object to call it on)
                               "struct S { int Size() { return 4; } int size = Size(); };"})
    {
        std::vector<cxxsnippets::Diagnostic> errors;
        auto m = cxxsnippets::Compile(source, "bad.cpp", {}, errors);
        Check(!m && !errors.empty(), "invalid source rejected");
    }
}
static void VariadicMacros()
{
    auto m = Build(R"(
        #define SUM(...) add(__VA_ARGS__)
        #define FIRST(a, ...) (a + add(__VA_ARGS__))
        int add(int a, int b) { return a + b; }
        int all() { return SUM(1, 2); }
        int named() { return FIRST(10, 3, 4); }
    )");
    Check(Get<int (*)()>(*m, "all")() == 3, "F(...) with two arguments");
    Check(Get<int (*)()>(*m, "named")() == 17, "F(a, ...) keeps a separate from __VA_ARGS__");
}
static void Bindings()
{
    auto m = Build(R"(
        #include <injector/injector.hpp>
        #include <Hooking.Patterns.h>
        #include <safetyhook.hpp>
        #include <windows.h>
        #include <cstdio>
        uint8_t bytes[]={0x74,0x10,0x53,0x53,0x6A,0x1B};
        void patch(){auto first=reinterpret_cast<uintptr_t>(bytes);injector::WriteMemory<uint8_t>(hook::range_pattern(first,first+sizeof(bytes),"74 10 53 53 6A 1B"),0xEB,true);}
        int memory(){int x=3;injector::WriteMemory(&x,17,false);return injector::ReadMemory<int>(&x,false);}
        int patterns(){hook::pattern p(reinterpret_cast<uintptr_t>(bytes),reinterpret_cast<uintptr_t>(bytes)+sizeof(bytes),"74 10 53 53 6A 1B");auto q=p;return q.count(1).size()+*q.get(0).get<uint8_t>();}
        int contexts(){return sizeof(SafetyHookContext);}
        int varargs(){char out[40];snprintf(out,sizeof(out),"%d %.1f",7,2.5);return out[0]+out[2]+out[4];}
    )");
    Check(Get<int (*)()>(*m, "memory")() == 17, "injector templates");
    Check(Get<int (*)()>(*m, "patterns")() == 117, "pattern values and methods");
    Check(Get<int (*)()>(*m, "contexts")() == (sizeof(void *) == 8 ? 408 : 176), "safetyhook context layout");
    Check(Get<int (*)()>(*m, "varargs")() == 158, "variadic float ABI");
    Get<void (*)()>(*m, "patch")();
    Check(*(unsigned char *)m->Find("bytes") == 0xeb, "Alien Isolation style range patch");
}
static void EdgeCases()
{
    auto m = Build(R"(
        #include <cstdint>
        #include <cstring>
        int compound(){int a[]={10,20};int i=0;a[i++]+=7;return i*100+a[0]+a[1];}
        int shifts(){unsigned int u=0xf0000000;int n=-16;return (u>>28)+(n>>2);}
        int signed_narrow(){signed char a=-3;unsigned char b=250;short c=-400;return a+b+c;}
        int pointers(){int a[4]={2,3,5,7};int* p=a+3;--p;return p-a+*p;}
        int jumps(int n){int sum=0;again: if(n==0)goto done;sum+=n--;goto again;done:return sum;}
        int __stdcall lambda_stdcall(){int (__stdcall *p)(int)=[](int x){return x+7;};return p(5);}
        int lambda_fastcall(){int (__fastcall *p)(int,int,int)=[](int a,int b,int c){return a+b+c;};return p(1,2,3);}
        int overload(int x){return x+10;} int overload(double x){return static_cast<int>(x)+20;}
        int overloaded_address(){int (*p)(double)=overload;return p(4.0);}
        int nested_array(){int a[2][3]={{2,3,5},{7,11,13}};int (*p)[3]=a;return p[1][2]+sizeof(a);}
        union U { uint64_t q;uint32_t d[2]; }; int unions(){U u{};u.q=0x1122334455667788ULL;return u.d[1]==0x11223344;}
        int large_frame(){char bytes[12000];bytes[0]=3;bytes[11999]=7;return bytes[0]+bytes[11999];}
        int const_pointer(){int x=5;int const* p=&x;return *p;}
        int null_pointer(){int* p=nullptr;return p==nullptr;}
        int strings(){return strlen(R"raw(hello!)raw")+sizeof(L"ab");}
        int locals(){typedef int X;{using X=long long;X v=1;}X v=3;return sizeof(v);}
        struct Copy { int x;int y; };int copies(){Copy a{3,4};Copy b=a;b=a;return b.x+b.y;}
        int state(int n){static int value=n;return ++value;}
        int auto_reference(){int x=3;auto& r=x;auto* p=&x;r+=2;return *p;}
        int float_truth(){return !(-0.0)+!(0.0f);}
        constexpr int ct_fib(int n){if(n<2)return n;return ct_fib(n-1)+ct_fib(n-2);}
        constexpr int ct_sum(int n){int sum=0;for(int i=0;i<n;++i)sum+=i;switch(n){case 4:return sum+1;default:return sum;}}
        int constant_functions(){int a[ct_fib(6)]{};int b[ct_sum(4)]{};return sizeof(a)+sizeof(b);}
        int ranks(long x){return 10;}int ranks(unsigned long x){return 20;}int ranks(unsigned int x){return 30;}int arithmetic_rank(){long a=3;unsigned int b=4;return ranks(a+b);}
        enum class E { A=3,B=4 };int enum_compare(){return E::A<E::B;}
        #if 0 && (1/0)
        #error short circuit broken
        #endif
        #if 1 ? 1 : (1/0)
        int preprocessing(){return 9;}
        #endif
    )");
    for (auto test : {std::pair{"compound", 137},
                      std::pair{"shifts", 11},
                      std::pair{"signed_narrow", -153},
                      std::pair{"pointers", 7},
                      std::pair{"lambda_stdcall", 12},
                      std::pair{"lambda_fastcall", 6},
                      std::pair{"overloaded_address", 24},
                      std::pair{"nested_array", 37},
                      std::pair{"unions", 1},
                      std::pair{"large_frame", 10},
                      std::pair{"const_pointer", 5},
                      std::pair{"null_pointer", 1},
                      std::pair{"strings", 12},
                      std::pair{"locals", 4},
                      std::pair{"preprocessing", 9},
                      std::pair{"copies", 7},
                      std::pair{"auto_reference", 5},
                      std::pair{"float_truth", 2},
                      std::pair{"arithmetic_rank", 20},
                      std::pair{"enum_compare", 1},
                      std::pair{"constant_functions", 60}})
        Check(Get<int (*)()>(*m, test.first)() == test.second, test.first);
    auto state = Get<int (*)(int)>(*m, "state");
    Check(state(5) == 6 && state(100) == 7, "static local initialization");
    Check(Get<int (*)(int)>(*m, "jumps")(10) == 55, "goto labels");
    for (const char *source :
         {"int f(){const int x=3;x=4;return x;}", "int f(){int const* p=nullptr;*p=4;return 0;}",
          "int f(){int const* p=nullptr;int* q=p;return 0;}", "int f(){int x=3;auto p=[](){return x;};return p();}",
          "int f(){goto done;int x=3;done:return x;}"})
    {
        std::vector<cxxsnippets::Diagnostic> errors;
        auto bad = cxxsnippets::Compile(source, "bad.cpp", {}, errors);
        Check(!bad && !errors.empty(), "invalid edge case rejected");
    }
}
extern "C" int case_add(int, int);
static void Lifetimes()
{
    Check(case_add(2, 3) == 5, "baseline before lifetime test");
    auto m = Build(R"(
        #include <safetyhook.hpp>
        extern "C" int case_add(int a,int b);
        SafetyHookInline g=safetyhook::create_inline(case_add,[](int a,int b){return g.call<int>(a*2,b*2);});
    )");
    Check(case_add(2, 3) == 10, "dynamic global hook initialization");
    m.reset();
    Check(case_add(2, 3) == 5, "automatic global handle cleanup");
    m = Build(R"(
        #include <safetyhook.hpp>
        extern "C" int case_add(int a,int b);
        int replace(int a,int b){return 99;}
        void discarded(){safetyhook::create_inline(case_add,replace);}
        int temporary(){return safetyhook::create_inline(case_add,replace).call<int>(2,3);}
    )");
    Get<void (*)()>(*m, "discarded")();
    Check(case_add(2, 3) == 5, "discarded temporary release");
    Check(Get<int (*)()>(*m, "temporary")() == 5, "temporary receiver invocation");
    Check(case_add(2, 3) == 5, "temporary receiver cleanup");
    MEMORY_BASIC_INFORMATION info{};
    auto f = m->Find("temporary");
    Check(VirtualQuery(f, &info, sizeof(info)) != 0 && info.Protect == PAGE_EXECUTE_READ, "code is RX");
#ifdef _WIN64
    DWORD64 base = 0;
    auto entry = RtlLookupFunctionEntry((DWORD64)f, &base, nullptr);
    Check(entry != nullptr, "unwind registration");
    auto unwind = (unsigned char *)(base + entry->UnwindData);
    size_t allocation = 0;
    if (unwind[2] > 1)
        allocation = ((size_t)unwind[6] | ((size_t)unwind[7] << 8)) * 8;
    alignas(16) uint64_t stack[2048]{};
    size_t frame = 1000;
    stack[frame] = 0x12345678;
    stack[frame + 1] = 0x87654321;
    CONTEXT context{};
    context.Rsp = (DWORD64)&stack[frame] - allocation;
    context.Rbp = (DWORD64)&stack[frame];
    context.Rip = base + entry->BeginAddress + unwind[1] + 1;
    PVOID handlerData = nullptr;
    DWORD64 establisher = 0;
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, context.Rip, entry, &context, &handlerData, &establisher, nullptr);
    Check(context.Rip == 0x87654321 && context.Rbp == 0x12345678 && context.Rsp == (DWORD64)&stack[frame + 2],
          "unwind restores caller context");
#endif
}
int CaseChild(const std::string &, const std::string &);
void UseCases();
void Differential();
// Paths are UTF-8, so snippets in folders outside the ANSI code page compile with relative includes
static void UnicodePaths()
{
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    std::wstring dir = std::wstring(temp) + L"cxxsnippets Кошмар 中文";
    std::wstring inc = dir + L"\\ф";
    CreateDirectoryW(dir.c_str(), nullptr);
    CreateDirectoryW(inc.c_str(), nullptr);
    auto write = [](const std::wstring &path, const char *text) {
        HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        DWORD n;
        WriteFile(h, text, (DWORD)strlen(text), &n, nullptr);
        CloseHandle(h);
    };
    write(inc + L"\\value.h", "#pragma once\nint value = 41;\n");
    write(dir + L"\\snippet.cxx", "#include \"\xd1\x84/value.h\"\nint answer() { return value + 1; }\n"); // \xd1\x84: U+0444 in UTF-8
    auto utf8 = [](const std::wstring &w) {
        std::string s(WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr), '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), (int)s.size(), nullptr, nullptr);
        return s;
    };
    std::vector<cxxsnippets::Diagnostic> errors;
    auto m = cxxsnippets::CompileFile(utf8(dir + L"\\snippet.cxx"), {}, errors);
    for (auto &e : errors)
        std::fprintf(stderr, "%s\n", e.ToString().c_str());
    Check(m != nullptr, "compile a snippet in a folder outside the ANSI code page");
    if (m)
        Check(Get<int (*)()>(*m, "answer")() == 42, "relative include from a folder outside the ANSI code page");
    DeleteFileW((inc + L"\\value.h").c_str());
    DeleteFileW((dir + L"\\snippet.cxx").c_str());
    RemoveDirectoryW(inc.c_str());
    RemoveDirectoryW(dir.c_str());
}

// Address of the access violation in fn(nullptr). No C++ objects allowed here because of __try.
static void *FaultAddress(int (*fn)(int *))
{
    void *at = nullptr;
    __try
    {
        fn(nullptr);
    }
    __except (at = (GetExceptionInformation())->ExceptionRecord->ExceptionAddress, EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return at;
}

// Source locations of code addresses, which crash reports use
static void Locations()
{
    auto m = Build("int first() { return 1; }\n"
                   "\n"
                   "int crashy(int *p)\n"
                   "{\n"
                   "    int a = 1;\n"
                   "    return *p + a;\n"
                   "}\n");
    auto crashy = Get<int (*)(int *)>(*m, "crashy");
    Check(m->Contains((void *)crashy), "Contains: function entry");
    Check(!m->Contains((void *)&Check), "Contains: host code");
    cxxsnippets::SourceLocation loc;
    Check(m->Locate((void *)crashy, loc), "Locate: function entry");
    Check(loc.function == "crashy" && loc.file == "test.cpp" && loc.line == 3, "Locate: entry is the declaration line");
    // run on a null pointer and locate the faulting load
    void *faultAt = FaultAddress(crashy);
    Check(faultAt != nullptr && m->Locate(faultAt, loc), "Locate: fault address");
    Check(loc.function == "crashy" && loc.line == 6, "Locate: the statement that faulted");
}

// Check() compiles without running anything
static void CheckOnly()
{
    std::vector<cxxsnippets::Diagnostic> errors;
    static volatile int marker = 0;
    cxxsnippets::Options o;
    o.symbols.push_back({"marker", (void *)&marker});
    Check(cxxsnippets::Check("extern int marker; int set() { marker = 5; return 0; } int x = set();", "check.cxx", o, errors),
          "Check: valid snippet");
    Check(marker == 0, "Check: global initializers do not run");
    Check(!cxxsnippets::Check("int f() { return undefined_name; }", "check.cxx", o, errors) && !errors.empty() &&
              errors[0].file == "check.cxx" && errors[0].line == 1,
          "Check: errors are reported with their location");
}

// Unload() releases hooks and reverts memory writes newest first. Code stays callable.
static volatile uint8_t patchTarget[8] = {1, 2, 3, 4, 5, 6, 7, 8};
static void Unloading()
{
    cxxsnippets::Options o;
    o.symbols.push_back({"patchTarget", (void *)patchTarget});
    auto m = Build("#include <injector/injector.hpp>\n"
                   "#include <cstdint>\n"
                   "extern uint8_t patchTarget[8];\n"
                   "int still() { return 42; }\n"
                   "void Init() {\n"
                   "    injector::WriteMemory<uint8_t>(&patchTarget[0], 0xAA, true);\n"
                   "    injector::WriteMemory<uint8_t>(&patchTarget[0], 0xBB, true);\n" // written twice, the original must come back
                   "    injector::MakeNOP(&patchTarget[2], 3, true);\n"
                   "}\n",
                   o);
    Check(m->RunInit(), "Unload: Init");
    Check(patchTarget[0] == 0xBB && patchTarget[2] == 0x90 && patchTarget[4] == 0x90, "Unload: writes applied");
    auto still = Get<int (*)()>(*m, "still");
    m->Unload();
    Check(m->Unloaded(), "Unload: state");
    Check(patchTarget[0] == 1 && patchTarget[2] == 3 && patchTarget[3] == 4 && patchTarget[4] == 5, "Unload: original bytes restored");
    Check(still() == 42, "Unload: the code stays callable (calls in progress can finish)");
    m->Unload(); // second call is a no-op
    Check(patchTarget[0] == 1, "Unload: idempotent");
}

// extern "C" T x; declares the host's variable, extern "C" { T x; } defines one
static volatile int hostArray[4] = {1, 2, 3, 4};
static void LinkageDeclarations()
{
    cxxsnippets::Options o;
    o.symbols.push_back({"hostArray", (void *)hostArray});
    auto m = Build("extern \"C\" int hostArray[4];\n"
                   "extern \"C\" { int ownArray[2] = {5, 6}; }\n"
                   "extern \"C\" int sum() { return ownArray[0] + ownArray[1]; }\n"
                   "void Init() { hostArray[1] = 21; }\n",
                   o);
    Check(m->RunInit() && hostArray[1] == 21, "extern \"C\" declaration binds to the host variable");
    Check(Get<int (*)()>(*m, "sum")() == 11, "extern \"C\" { } defines, and functions with bodies are defined");
    hostArray[1] = 2;
}

static void Crashes()
{
    // Global init crash becomes a compile error at the faulting line, with everything undone
    static volatile uint8_t target = 7;
    cxxsnippets::Options o;
    o.symbols.push_back({"target", (void *)&target});
    std::vector<cxxsnippets::Diagnostic> errors;
    auto m = cxxsnippets::Compile("#include <injector/injector.hpp>\n"
                                  "#include <cstdint>\n"
                                  "extern uint8_t target;\n"
                                  "int patch() { injector::WriteMemory<uint8_t>((void *)&target, 9, true); return 0; }\n"
                                  "int crash() { int *p = nullptr; return *p; }\n"
                                  "int a = patch();\n"
                                  "int b = crash();\n",
                                  "init.cxx", o, errors);
    Check(m == nullptr && errors.size() == 1, "global initialization crash: compile error");
    Check(errors[0].file == "init.cxx" && errors[0].line == 5 && errors[0].message.find("crashed during global initialization") == 0,
          "global initialization crash: located at the faulting statement");
    Check(target == 7, "global initialization crash: earlier writes undone");

    // RunInit(&crash) reports an Init crash instead of raising it
    auto n = Build("void helper(int *p) { *p = 1; }\n"
                   "void Init()\n"
                   "{\n"
                   "    helper(nullptr);\n"
                   "}\n");
    cxxsnippets::Crash crash;
    Check(!n->RunInit(&crash), "Init crash: RunInit fails");
    Check(crash.code == EXCEPTION_ACCESS_VIOLATION, "Init crash: exception code");
    Check(crash.where.function == "helper" && crash.where.line == 1, "Init crash: located in the function that faulted");
}

// A typical widescreen fix against "game" code here and a system DLL: a __thiscall method found
// by pattern and fully replaced, a GetProcAddress'd export hooked with the original called for
// other cases, and Windows APIs missing from the built-in <windows.h>, one from a DLL the snippet loads.
#pragma section(".text$ual", read, execute)
#ifdef _WIN64
// int Device::SetRes(int w, int h) { width = w; height = h; return 1; }  (this in rcx)
__declspec(allocate(".text$ual")) static const unsigned char gameCode[] = {
    0xCC, 0x55, 0x41, 0x4C, 0x53, 0x45, 0x54, 0x52, // marker
    0x89, 0x11, 0x44, 0x89, 0x41, 0x04, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3};
#else
// same method as __thiscall: this in ecx, ret 8
__declspec(allocate(".text$ual")) static const unsigned char gameCode[] = {
    0xCC, 0x55, 0x41, 0x4C, 0x53, 0x45, 0x54, 0x52, // marker
    0x8B, 0x44, 0x24, 0x04, 0x89, 0x01, 0x8B, 0x44, 0x24, 0x08, 0x89, 0x41, 0x04, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC2, 0x08, 0x00};
#endif
static int CallSetRes(int *device, int w, int h)
{
#ifdef _WIN64
    return reinterpret_cast<int (*)(int *, int, int)>((void *)(gameCode + 8))(device, w, h);
#else
    return reinterpret_cast<int(__fastcall *)(int *, void *, int, int)>((void *)(gameCode + 8))(device, nullptr, w, h);
#endif
}
static void GameStyleHooks()
{
    int device[2] = {};
    Check(CallSetRes(device, 640, 480) == 1 && device[0] == 640, "game code runs before the hooks");
    auto m = Build(R"(#include <windows.h>
#include <safetyhook.hpp>
#include <Hooking.Patterns.h>
// Not in the built-in <windows.h>: declared here, bound to the exports of the loaded DLLs.
extern "C" {
int WINAPI GetSystemMetrics(int index);
DWORD WINAPI GetTickCount();
UINT WINAPI GetPrivateProfileIntA(LPCSTR section, LPCSTR key, int fallback, LPCSTR path);
HMODULE WINAPI LoadLibraryA(LPCSTR name);
int __cdecl wsprintfA(char *buffer, char const *format, ...);
}
typedef DWORD(WINAPI *GetFileVersionInfoSizeA_t)(LPCSTR path, DWORD *handle);

int width = 0;
int versionSize = 0;
char text[64];
SafetyHookInline metricsHook{};
SafetyHookInline setResHook{};

// Partial: changes one case, calls the original for the rest.
int WINAPI GetSystemMetricsHook(int index)
{
    if (index == 0) // SM_CXSCREEN
        return width;
    return metricsHook.stdcall<int>(index);
}

// Full replacement of a __thiscall method: the original never runs.
#ifdef _WIN64
int SetResHook(int *self, int w, int h)
#else
int __fastcall SetResHook(int *self, void *edx, int w, int h)
#endif
{
    self[0] = w * 2;
    self[1] = h + (GetTickCount() != 0 ? 1 : 0);
    return 7;
}

void Init()
{
    width = GetPrivateProfileIntA("Main", "Width", 3440, "Z:\nonexistent\fix.ini");
    wsprintfA(text, "%dx%d %s", width, 1440, "ultrawide");

    HMODULE user32 = GetModuleHandleA("user32.dll");
    metricsHook = safetyhook::create_inline(GetProcAddress(user32, "GetSystemMetrics"), GetSystemMetricsHook);

    void *setRes = hook::module_pattern(GetModuleHandleA(nullptr), "CC 55 41 4C 53 45 54 52", 8);
    setResHook = safetyhook::create_inline(setRes, SetResHook);

    HMODULE version = LoadLibraryA("version.dll");
    auto size = (GetFileVersionInfoSizeA_t)GetProcAddress(version, "GetFileVersionInfoSizeA");
    DWORD unused = 0;
    versionSize = (int)size("kernel32.dll", &unused);
}
)");
    Check(m->RunInit(), "game hooks: Init");
    Check(*(int *)m->Find("width") == 3440, "WinAPI not in <windows.h>: GetPrivateProfileIntA");
    Check(std::string((char *)m->Find("text")) == "3440x1440 ultrawide", "variadic WinAPI: wsprintfA");
    Check(*(int *)m->Find("versionSize") > 0, "function from a DLL the snippet loads: version.dll");
    Check(GetSystemMetrics(SM_CXSCREEN) == 3440, "DLL export hooked: changed case");
    Check(GetSystemMetrics(SM_CYSCREEN) > 0 && GetSystemMetrics(SM_CYSCREEN) != 3440, "DLL export hooked: original for other cases");
    Check(CallSetRes(device, 800, 600) == 7 && device[0] == 1600 && device[1] == 601, "__thiscall method replaced, original not called");
    m->Unload();
    Check(GetSystemMetrics(SM_CXSCREEN) != 3440, "unload: DLL hook removed");
    Check(CallSetRes(device, 1024, 768) == 1 && device[0] == 1024 && device[1] == 768, "unload: game code restored");
}

// MakeCALL/MakeJMP/MakeCALLTrampoline from snippet code over 2 GB from the patch site, usual on x64.
// The rel32 goes through a jump stub.
// int Caller() { return Callee(); }  int Callee() { return 1; }
__declspec(allocate(".text$ual")) static const unsigned char callSite[] = {
    0xCC, 0x55, 0x41, 0x4C, 0x43, 0x41, 0x4C, 0x4C, // marker
    0xE8, 0x01, 0x00, 0x00, 0x00, 0xC3,             // Caller: call Callee; ret
    0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3};            // Callee: mov eax, 1; ret
static int farChecks = 0;
static void FarCalls()
{
    auto caller = reinterpret_cast<int (*)()>((void *)(callSite + 8));
    Check(caller() == 1, "far call: game code runs");
    for (auto name : {"MakeCALL", "MakeCALLTrampoline", "MakeJMP", "MakeJMPTrampoline"})
    {
        std::string jmp = std::string(name).find("JMP") != std::string::npos ? "1" : "0";
        auto m = Build("#include <windows.h>\n#include <injector/injector.hpp>\n#include <Hooking.Patterns.h>\n"
                       "typedef int (*Fn)();\nFn original = nullptr;\n"
                       "int Replacement() { return original() + 41; }\n"
                       "int Jumped() { return 7; }\n"
                       "void Init() {\n"
                       "    void *site = hook::module_pattern(GetModuleHandleA(nullptr), \"CC 55 41 4C 43 41 4C 4C\", 8);\n"
                       "    if (" + jmp + ") site = (char *)site + 6;\n" // jmp replaces Callee's code
                       "    original = (Fn)((char *)site + 5 + *(int *)((char *)site + 1));\n"
                       "    if (" + jmp + ") original = nullptr;\n"
                       "    injector::" + name + "(site, " + (jmp == "1" ? "Jumped" : "Replacement") + ", true);\n"
                       "}\n");
        auto distance = (intptr_t)m->Find("Replacement") - (intptr_t)(callSite + 13);
        Check(m->RunInit(), name);
#ifdef _WIN64
        if (distance != (int32_t)distance)
            ++farChecks; // the stub path ran
#endif
        int expected = jmp == "1" ? 7 : 42;
        Check(caller() == expected, (std::string(name) + ": redirected call works (distance " +
                                     std::to_string((long long)distance) + ")").c_str());
        m->Unload();
        Check(caller() == 1, (std::string(name) + ": restored").c_str());
    }
#ifdef _WIN64
    std::printf("far calls: %d of 4 went through a jump stub\n", farChecks);
#endif
}

// Found by the examples: functional casts, ?: with pointers, static_assert, offsetof
static void ExampleFeatures()
{
    auto m = Build(R"(
        #include <Hooking.Patterns.h>
        int hits = 0;
        int scan() { hook::pattern("DE AD 13 37 C0 DE 99 98").for_each_result([](hook::pattern_match m) { ++hits; }); return hits; }
        int choose(int which) {
            int x = 5;
            void *p = which ? (void *)&x : nullptr;
            int *q = which > 1 ? nullptr : &x;
            char const *r = which ? "abc" : 0;
            void const *s = which ? (void const *)r : (void *)q;
            return (p ? 1 : 0) + (q ? 10 : 0) + (r ? 100 : 0) + (s ? 1000 : 0);
        }
        struct Inner { short a; int b[3]; };
        struct Outer { char c; Inner inner; double d; static_assert(sizeof(int) == 4, "int"); };
        static_assert(offsetof(Outer, inner) == 4, "inner");
        static_assert(offsetof(Outer, inner.b[2]) == 16);
        int offsets() { static_assert(offsetof(Outer, d) == 24, "d"); return (int)offsetof(Outer, inner.b[1]); }
    )");
    Check(Get<int (*)()>(*m, "scan")() == 0, "functional cast with member call as a statement");
    Check(Get<int (*)(int)>(*m, "choose")(1) == 1111 && Get<int (*)(int)>(*m, "choose")(0) == 1010 &&
              Get<int (*)(int)>(*m, "choose")(2) == 1101,
          "?: with pointer, nullptr and 0 operands");
    Check(Get<int (*)()>(*m, "offsets")() == 12, "offsetof with nested members and subscripts");

    std::vector<cxxsnippets::Diagnostic> errors;
    Check(!cxxsnippets::Check("static_assert(sizeof(int) == 8, \"too small\");", "a.cxx", {}, errors) &&
              errors[0].message.find("too small") != std::string::npos,
          "static_assert failure reports its message");
    errors.clear();
    Check(!cxxsnippets::Check("int f(int *p, float *q) { return (p ? p : q) != 0; }", "a.cxx", {}, errors),
          "?: with unrelated object pointers is an error");
}

// examples/snippets/*.cxx patch made-up addresses, so they are only compiled, on both architectures
static void Examples()
{
    std::string dir = std::string(CXXSNIPPETS_TEST_ROOT) + "/../../examples/snippets/";
    WIN32_FIND_DATAA data;
    HANDLE find = FindFirstFileA((dir + "*.cxx").c_str(), &data);
    Check(find != INVALID_HANDLE_VALUE, "examples: folder");
    int count = 0, failed = 0;
    do
    {
        std::vector<cxxsnippets::Diagnostic> errors;
        bool ok = cxxsnippets::CheckFile(dir + data.cFileName, {}, errors);
        for (auto &e : errors)
            std::fprintf(stderr, "%s\n", e.ToString().c_str());
        if (!ok)
        {
            std::fprintf(stderr, "FAIL: example %s\n", data.cFileName);
            ++failed;
        }
        ++checks;
        ++count;
    } while (FindNextFileA(find, &data));
    FindClose(find);
    Check(failed == 0, "examples compile");
    Check(count >= 13, "examples: all found");
}

int main(int argc, char **argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    if (argc == 4 && std::string(argv[1]) == "--case")
        return CaseChild(argv[2], argv[3]);
    Basic();
    Features();
    ABI();
    Preprocessor();
    UnicodePaths();
    Locations();
    CheckOnly();
    Unloading();
    GameStyleHooks();
    FarCalls();
    ExampleFeatures();
    Examples();
    Crashes();
    LinkageDeclarations();
    Bindings();
    EdgeCases();
    Lifetimes();
    Errors();
    VariadicMacros();
    Differential();
    UseCases();
    std::printf("PASS: %d unit checks (%s)\n", checks, sizeof(void *) == 8 ? "x64" : "Win32");
}
