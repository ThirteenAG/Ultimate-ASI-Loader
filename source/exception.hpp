#pragma once
/*
 * Unhandled Exception Tracer
 * by LINK/2012 <dma_2012@hotmail.com>
 *
 *  This source code is offered for use in the public domain. You may
 *  use, modify or distribute it freely.
 *
 *  This code is distributed in the hope that it will be useful but
 *  WITHOUT ANY WARRANTY. ALL WARRANTIES, EXPRESS OR IMPLIED ARE HEREBY
 *  DISCLAIMED. This includes but is not limited to warranties of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <cassert>
#include <DbgHelp.h>
#pragma comment(lib, "Dbghelp.lib")

 /*
  *  Special Note:
  *      Try not to allocate any memory in this file!
  *      Allocation after a exception may not be a good idea...
  */

#define LODWORD(_qw)    ((DWORD)(_qw))
#define HIDWORD(_qw)    ((DWORD)(((_qw) >> 32) & 0xffffffff))

  // General constants
static const int sizeof_word = sizeof(void*);           // Size of a CPU word (4 bytes on x86)
static const int max_chars_per_print = MAX_PATH + 256;  // Max characters per Print() call
static const int symbol_max = 256;                      // Max size of a symbol (func symbol, var symbol, etc)
static const int max_static_buffer = 4096;              // Max static buffer for logging

// Stackdump constants
static const int stackdump_max_words = 60;              // max number of CPU words that the stackdump should dump
static const int stackdump_words_per_line = 6;          // max CPU words in a single line
static const int stackdump_line_count = (stackdump_max_words / stackdump_words_per_line) + 1;

// Backtrace constants
static const int max_backtrace_ever = 100;
static const int max_backtrace = 20;

// Maximum log size constants
static const int max_logsize_basic = (MAX_PATH + 200);      // module path + other text
static const int max_logsize_regs = 32 + (4 * 4 * 28);     // info + (regsPerLine * numLines * charsPerReg)
static const int max_logsize_stackdump = 32 + 80 + (stackdump_line_count * 32) + (10 * stackdump_words_per_line * stackdump_line_count);
static const int max_logsize_backtrace = 32 + max_backtrace_ever * (MAX_PATH + symbol_max + 90);
static const int max_logsize_ever = 32 + max_logsize_basic + max_logsize_regs + max_logsize_stackdump + max_logsize_backtrace;

// Internal
class ExceptionTracer;
class StackTrace;
static HMODULE GetModuleFromAddress(LPVOID address);
static const char* GetExceptionCodeString(unsigned int code);
static const char* FindModuleName(HMODULE module, char* output, DWORD size);
static int LogException(char* buffer, size_t max, LPEXCEPTION_POINTERS pException, bool bLogRegisters, bool bLogStack, bool bLogBacktrace);
static LPTOP_LEVEL_EXCEPTION_FILTER PrevFilter = nullptr;
static void(*ExceptionCallback)(const char* buffer) = nullptr;

// Exportable
int InstallExceptionCatcher(void(*OnException)(const char* log));

/*
 *  ExceptionTrace
 *      This class is responssible for tracing all possible informations about an LPEXCEPTION_POINTER
 */
class ExceptionTracer
{
public:
    ExceptionTracer(char* buffer, size_t max, LPEXCEPTION_POINTERS pException);
    void PrintUnhandledException();
    void PrintRegisters();
    void PrintStackdump();
    void PrintBacktrace();

    void EnterScope();
    void LeaveScope();
    void Print(const char* fmt, ...);
    void NewLine() { Print("\n%s", spc); }

protected:
    EXCEPTION_POINTERS& exception;
    EXCEPTION_RECORD& record;
    CONTEXT& context;
    HMODULE module;

    char* buffer;       // Logging buffer
    size_t len;         // Logged length
    size_t max;         // Maximum we can log in that buffer

    char spc[(10 * 4) + 1]; // Scope/spacing buffer, 4 spaces per scope, max 10 scopes
    size_t nspc;        // Number spaces used up there
};

/*
 *  StackTracer
 *      Responssible for backtracing an stack from a context
 */
class StackTracer
{
public:
    struct Trace
    {
        // The following values may be null (any)
        HMODULE module;     // The module the func related to this frame is located
        void *pc;           // Program counter at func related to this frame (EIP)
        void *ret;          // Return address for the frame
        void *frame;        // The frame address (EBP)
        void *stack;        // The stack pointer at the frame (ESP)
    };

    StackTracer(const CONTEXT& context);
    Trace* Walk();

private:
    Trace trace;
    DWORD old_options;
    CONTEXT context;
    STACKFRAME64 frame;
};

/*
 *  TheUnhandledExceptionFilter
 *      Logs an unhandled exception
 */
static LONG CALLBACK TheUnhandledExceptionFilter(LPEXCEPTION_POINTERS pException)
{
    // Logs exception into buffer and calls the callback
    auto Log = [pException](char* buffer, size_t size, bool reg, bool stack, bool trace)
    {
        if (LogException(buffer, size, (LPEXCEPTION_POINTERS)pException, reg, stack, trace))
            ExceptionCallback(buffer);
    };

    // Try to make a very descriptive exception, for that we need to malloc a huge buffer...
    if (auto buffer = (char*)malloc(max_logsize_ever))
    {
        Log(buffer, max_logsize_ever, true, true, true);
        free(buffer);
    }
    else
    {
        // Use a static buffer, no need for any allocation
        static const auto size = max_logsize_basic + max_logsize_regs + max_logsize_stackdump;
        static char static_buf[size];
        static_assert(size <= max_static_buffer, "Static buffer is too big");

        Log(buffer = static_buf, sizeof(static_buf), true, true, false);
    }

    // Continue exception propagation
    return (PrevFilter ? PrevFilter(pException) : EXCEPTION_CONTINUE_SEARCH);  // I'm not really sure about this return
}

/*
 *  InstallExceptionCatcher
 *      Installs a exception handler to call the specified callback when it happens with human readalbe information.
 */
int InstallExceptionCatcher(void(*cb)(const char* log))
{
    PrevFilter = SetUnhandledExceptionFilter(TheUnhandledExceptionFilter);
    ExceptionCallback = cb;
    return 1;
}

/*
 * LogException
 *      Takes an LPEXCEPTION_POINTERS and transforms in a string that is put in the logging steam
 */
static int LogException(char* buffer, size_t max, LPEXCEPTION_POINTERS pException, bool bLogRegisters, bool bLogStack, bool bLogBacktrace)
{
    ExceptionTracer trace(buffer, max, pException);
    trace.PrintUnhandledException();
    trace.EnterScope();
    if (bLogRegisters) trace.PrintRegisters();
    if (bLogStack) trace.PrintStackdump();
    if (bLogBacktrace) trace.PrintBacktrace();
    trace.LeaveScope();
    return 1;
}

/*
 *  ExceptionTracer
 *      Contructs a exception trace object, responssible for tracing informations about an exception
 */
ExceptionTracer::ExceptionTracer(char* buffer, size_t max, LPEXCEPTION_POINTERS pException) :
    buffer(buffer), exception(*pException), record(*pException->ExceptionRecord), context(*pException->ContextRecord)
{
    this->buffer = buffer;
    this->buffer[this->len = 0] = 0;
    this->spc[this->nspc = 0] = 0;
    this->max = max;

    // Acquiere common information that we'll access
    this->module = GetModuleFromAddress(record.ExceptionAddress);
}

/*
 *  Print
 *      Prints some formated text into the logging buffer
 */
void ExceptionTracer::Print(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    if ((this->max - this->len) > max_chars_per_print)
        this->len += vsprintf(&this->buffer[len], fmt, va);
    va_end(va);
}

/*
 *  EnterScope
 *      Enters a new scope in the logging buffer (scope is related to indentation)
 *      This also prints a new line
 */
void ExceptionTracer::EnterScope()
{
    nspc += 4;
    spc[nspc - 4] = ' ';
    spc[nspc - 3] = ' ';
    spc[nspc - 2] = ' ';
    spc[nspc - 1] = ' ';
    spc[nspc - 0] = 0;
    NewLine();
}

/*
 *  LeaveScope
 *      Leaves the scope
 */
void ExceptionTracer::LeaveScope()
{
    assert(nspc > 0);
    nspc -= 4;
    spc[nspc] = 0;
    NewLine();
}

/*
 *  PrintUnhandledException
 *      Prints the well known "Unhandled exception at ..." into the logging buffer
 */
void ExceptionTracer::PrintUnhandledException()
{
    char module_name[MAX_PATH];
    auto dwExceptionCode = record.ExceptionCode;
    uintptr_t address = (uintptr_t)record.ExceptionAddress;

    // Find out our module name for logging
    if (!this->module || !GetModuleFileNameA(this->module, module_name, sizeof(module_name)))
        strcpy(module_name, "unknown");

    // Log the exception in a similar format similar to debuggers format
    Print("Unhandled exception at 0x%p in %s", address, FindModuleName(module, module_name, sizeof(module_name)));
    if (module) Print(" (+0x%x)", address - (uintptr_t)(module));
    Print(": 0x%X: %s", dwExceptionCode, GetExceptionCodeString(dwExceptionCode));

    // If exception is IN_PAGE_ERROR or ACCESS_VIOLATION, we have additional information such as an address
    if (dwExceptionCode == EXCEPTION_IN_PAGE_ERROR || dwExceptionCode == EXCEPTION_ACCESS_VIOLATION)
    {
        auto rw = (DWORD)record.ExceptionInformation[0];  // read or write?
        auto addr = (ULONG_PTR)record.ExceptionInformation[1];  // which address?

        Print(" %s 0x%p",
            rw == 0 ? "reading location" : rw == 1 ? "writing location" : rw == 8 ? "DEP at" : "",
            addr);

        // IN_PAGE_ERROR have another information...
        if (dwExceptionCode == EXCEPTION_IN_PAGE_ERROR)
        {
            NewLine();
            Print("Underlying NTSTATUS code that resulted in the exception is 0x%p",
                record.ExceptionInformation[2]);
        }
    }

    Print(".");
}

/*
 *  PrintRegisters
 *      Prints the content of the assembly registers into the logging buffer
 */
void ExceptionTracer::PrintRegisters()
{
    int regs_in_line = 0;       // Amount of registers currently printed on this line

    // Prints a register, followed by spaces
    auto PrintRegister = [this, &regs_in_line](const char* reg_name, size_t reg_value, const char* spaces)
    {
        Print("%s: 0x%p%s", reg_name, reg_value, spaces);
        if (++regs_in_line >= 4) { this->NewLine(); regs_in_line = 0; }
    };

    auto PrintFloatRegister = [this, &regs_in_line](const char* reg_name, int reg_num, uint32_t reg_value1, uint32_t reg_value2, uint32_t reg_value3, uint32_t reg_value4)
    {
        Print("%s%02d: 0x%08X 0x%08X 0x%08X 0x%08X  [ %f %f %f %f ]", reg_name, reg_num, reg_value1, reg_value2, reg_value3, reg_value4,
            *(float*)&reg_value1, *(float*)&reg_value2, *(float*)&reg_value3, *(float*)&reg_value4);
        if (++regs_in_line >= 1) { this->NewLine(); regs_in_line = 0; }
    };

    // Prints a general purposes register
    auto PrintIntRegister = [PrintRegister](const char* reg_name, size_t reg_value)
    {
        PrintRegister(reg_name, reg_value, "  ");
    };

    // Prints a segment register
    auto PrintSegRegister = [PrintRegister](const char* reg_name, size_t reg_value)
    {
        PrintRegister(reg_name, reg_value, "   ");
    };

    Print("Register dump:");
    EnterScope();
    {
        // Print main general purposes registers
        if (context.ContextFlags & CONTEXT_INTEGER)
        {
#if !_M_X64
            PrintIntRegister("EAX", context.Eax);
            PrintIntRegister("EBX", context.Ebx);
            PrintIntRegister("ECX", context.Ecx);
            PrintIntRegister("EDX", context.Edx);
            PrintIntRegister("EDI", context.Edi);
            PrintIntRegister("ESI", context.Esi);
#else
            PrintIntRegister("RAX", context.Rax);
            PrintIntRegister("RCX", context.Rcx);
            PrintIntRegister("RDX", context.Rdx);
            PrintIntRegister("RBX", context.Rbx);
            PrintIntRegister("RBP", context.Rbp);
            PrintIntRegister("RSI", context.Rsi);
            PrintIntRegister("RDI", context.Rdi);
            PrintIntRegister("R08", context.R8);
            PrintIntRegister("R09", context.R9);
            PrintIntRegister("R10", context.R10);
            PrintIntRegister("R11", context.R11);
            PrintIntRegister("R12", context.R12);
            PrintIntRegister("R13", context.R13);
            PrintIntRegister("R14", context.R14);
            PrintIntRegister("R15", context.R15);
#endif
        }

        // Print control registers
        if (context.ContextFlags & CONTEXT_CONTROL)
        {
#if !_M_X64
            PrintIntRegister("EBP", context.Ebp);
            PrintIntRegister("EIP", context.Eip);
            PrintIntRegister("ESP", context.Esp);
            PrintIntRegister("EFL", context.EFlags);
            this->NewLine(); this->NewLine(); regs_in_line = 0;
            PrintSegRegister("CS", context.SegCs);
            PrintSegRegister("SS", context.SegSs);
#else
            PrintIntRegister("RIP", context.Rip);
            PrintIntRegister("RSP", context.Rsp);
            PrintIntRegister("EFL", context.EFlags);
            this->NewLine(); this->NewLine(); regs_in_line = 0;
            PrintSegRegister("CS", context.SegCs);
            PrintSegRegister("SS", context.SegSs);
#endif
        }

        this->NewLine(); regs_in_line = 0;

        // Print segment registers
        if (context.ContextFlags & CONTEXT_SEGMENTS)
        {
            PrintSegRegister("GS", context.SegGs);
            PrintSegRegister("FS", context.SegFs);
            this->NewLine(); regs_in_line = 0;
            PrintSegRegister("ES", context.SegEs);
            PrintSegRegister("DS", context.SegDs);
        }

        this->NewLine(); this->NewLine(); regs_in_line = 0;

        // Print floating point registers
        if (context.ContextFlags & CONTEXT_FLOATING_POINT)
        {
            for (int i = 0; i < 8; i++)
            {
#if !_M_X64
                auto f = *(M128A*)&(context.FloatSave.RegisterArea[i * 10]);
                PrintFloatRegister("ST", i, LODWORD(f.Low), HIDWORD(f.Low), LODWORD(f.High), HIDWORD(f.High));
#else
                PrintFloatRegister("ST", i,
                    LODWORD(context.FltSave.FloatRegisters[i].Low), HIDWORD(context.FltSave.FloatRegisters[i].Low),
                    LODWORD(context.FltSave.FloatRegisters[i].High), HIDWORD(context.FltSave.FloatRegisters[i].High));
#endif
            }

            this->NewLine();

            for (int i = 0; i < 16; i++)
            {
#if !_M_X64
                auto f = *(M128A*)&(context.ExtendedRegisters[(i + 10) * 16]);
                PrintFloatRegister("XMM", i, LODWORD(f.Low), HIDWORD(f.Low), LODWORD(f.High), HIDWORD(f.High));

                if (i >= 7)
                    break;
#else
                PrintFloatRegister("XMM", i,
                    LODWORD(context.FltSave.XmmRegisters[i].Low), HIDWORD(context.FltSave.XmmRegisters[i].Low),
                    LODWORD(context.FltSave.XmmRegisters[i].High), HIDWORD(context.FltSave.XmmRegisters[i].High));
#endif
            }
        }
    }
    LeaveScope();
}

/*
 *  PrintStackdump
 *      Prints the content of the stack into the logging buffer
 */
void ExceptionTracer::PrintStackdump()
{
    // We need the ESP of the exception context to execute a stack dump, make sure we have access to it
    if ((context.ContextFlags & CONTEXT_CONTROL) == 0)
        return;

    static const auto align = sizeof_word;      // Stack aligment
    static const auto max_words_in_line_magic = stackdump_words_per_line + 10;

    MEMORY_BASIC_INFORMATION mbi;
#if !_M_X64
    uintptr_t base, bottom, top = (uintptr_t)context.Esp;
#else
    uintptr_t base, bottom, top = (uintptr_t)context.Rsp;
#endif
    auto words_in_line = max_words_in_line_magic;

    // Finds the bottom of the stack from it's base pointer
    // Note: mbi will get overriden on this function
    auto GetStackBottom = [&mbi](uintptr_t base)
    {
        VirtualQuery((void*)base, &mbi, sizeof(mbi));                               // Find uncommited region of the stack
        VirtualQuery((char*)mbi.BaseAddress + mbi.RegionSize, &mbi, sizeof(mbi));   // Find guard page
        VirtualQuery((char*)mbi.BaseAddress + mbi.RegionSize, &mbi, sizeof(mbi));   // Find commited region of the stack
        auto last = (uintptr_t)mbi.BaseAddress;
        return (base + (last - base) + mbi.RegionSize);                             // base + distanceToLastRegion + lastRegionSize
    };

    // Prints an CPU word at the specified stack address
    auto PrintWord = [this, &words_in_line](uintptr_t addr)
    {
        if (words_in_line++ >= stackdump_words_per_line)
        {
            // Print new line only if it's not the first time we enter here (i.e. words_in_line has magical value)
            if (words_in_line != max_words_in_line_magic + 1) NewLine();
            words_in_line = 1;
            Print("0x%p: ", addr);
        }
        Print(" %p", *(size_t*)addr);
    };

    Print("Stack dump:");
    EnterScope();
    {
        // Makes sure the pointer at top (ESP) is valid and readable memory
        if (VirtualQuery((void*)(top), &mbi, sizeof(mbi))
            && (mbi.State & MEM_COMMIT)
            && (mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_READWRITE | PAGE_READONLY)) != 0)
        {
            base = (uintptr_t)mbi.AllocationBase;          // Base of the stack (uncommited)
            bottom = GetStackBottom(base);                 // Bottom of the stack (commited)

            // Align the stack top (esp) in a 4 bytes boundary
            auto remainder = top % align;
            uintptr_t current = remainder ? top + (align - remainder) : top;

            // on x86 stack grows downward! (i.e. from bottom to base)
            for (int n = 0; n < stackdump_max_words && current < bottom; ++n, current += align)
                PrintWord(current);

            NewLine();
            Print("base: 0x%p   top: 0x%p   bottom: 0x%p", base, top, bottom);
            NewLine();
        }
    }
    LeaveScope();
}

/*
 *  PrintBacktrace
 *      Prints a call backtrace into the logging buffer
 */
void ExceptionTracer::PrintBacktrace()
{
    StackTracer tracer(this->context);

    char module_name[MAX_PATH];
    char sym_buffer[sizeof(SYMBOL_INFO) + symbol_max];

    int backtrace_count = 0;        // Num of frames traced
    bool has_symbol_api = false;    // True if we have the symbol API available for use
    DWORD old_options;              // Saves old symbol API options

    SYMBOL_INFO& symbol = *(SYMBOL_INFO*)sym_buffer;
    symbol.SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol.MaxNameLen = symbol_max;

    // Tries to get the symbol api
    if (SymInitialize(GetCurrentProcess(), 0, TRUE))
    {
        has_symbol_api = true;
        old_options = SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_NO_PROMPTS | SYMOPT_FAIL_CRITICAL_ERRORS);
    }

    Print("Backtrace (may be wrong):");
    EnterScope();
    {
        // Walks on the stack until there's no frame to trace or we traced 'max_backtrace' frames
        while (auto trace = tracer.Walk())
        {
            if (++backtrace_count >= max_backtrace)
                break;

            bool has_sym = false;   // This EIP has a symbol associated with it?
            DWORD64 displacement;   // EIP displacement relative to symbol

            // If we have access to the symbol api, try to get symbol name from pc (eip)
            if (has_symbol_api)
                has_sym = trace->pc ? !!SymFromAddr(GetCurrentProcess(), (DWORD64)trace->pc, &displacement, &symbol) : false;

            // Print everything up, this.... Ew, this looks awful!
            Print(backtrace_count == 1 ? "=>" : "  ");                          // First line should have '=>' to specify where it crashed
            Print("0x%p ", trace->pc);                                          // Print EIP at frame
            if (has_sym) Print("%s+0x%x ", symbol.Name, (DWORD)displacement);   // Print frame func symbol
            Print("in %s (+0x%x) ",                                             // Print module
                trace->module ? FindModuleName(trace->module, module_name, sizeof(module_name)) : "unknown",
                (uintptr_t)(trace->pc) - (uintptr_t)(trace->module) // Module displacement
            );
            if (trace->frame) Print("(0x%p) ", trace->frame);                   // Print frame pointer

            NewLine();
        }
    }
    LeaveScope();

    // Cleanup the symbol api
    if (has_symbol_api)
    {
        SymSetOptions(old_options);
        SymCleanup(GetCurrentProcess());
    }
}

/*
 * GetExceptionCodeString
 *      Returns an description by an exception code
 */
static const char* GetExceptionCodeString(unsigned int code)
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:         return "Access violation";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "Array bounds exceeded";
    case EXCEPTION_BREAKPOINT:               return "Breakpoint exception";
    case EXCEPTION_DATATYPE_MISALIGNMENT:    return "Data type misalignment exception";
    case EXCEPTION_FLT_DENORMAL_OPERAND:     return "Denormal float operand";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "Floating-point division by zero";
    case EXCEPTION_FLT_INEXACT_RESULT:       return "Floating-point inexact result";
    case EXCEPTION_FLT_INVALID_OPERATION:    return "Floating-point invalid operation";
    case EXCEPTION_FLT_OVERFLOW:             return "Floating-point overflow";
    case EXCEPTION_FLT_STACK_CHECK:          return "Floating-point stack check";
    case EXCEPTION_FLT_UNDERFLOW:            return "Floating-point underflow";
    case EXCEPTION_ILLEGAL_INSTRUCTION:      return "Illegal instruction.";
    case EXCEPTION_IN_PAGE_ERROR:            return "In page error";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "Integer division by zero";
    case EXCEPTION_INT_OVERFLOW:             return "Integer overflow";
    case EXCEPTION_INVALID_DISPOSITION:      return "Invalid disposition";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "Non-continuable exception";
    case EXCEPTION_PRIV_INSTRUCTION:         return "Privileged instruction";
    case EXCEPTION_SINGLE_STEP:              return "Single step exception";
    case EXCEPTION_STACK_OVERFLOW:           return "Stack overflow";
    default:                                 return "NO_DESCRIPTION";
    }
}

/*
 * FindModuleName
 *      Finds module filename or "unknown"
 */
static const char* FindModuleName(HMODULE module, char* output, DWORD maxsize)
{
    if (GetModuleFileNameA(module, output, maxsize))
    {
        // Finds the filename part in the output string
        char* filename = strrchr(output, '\\');
        if (!filename) filename = strrchr(output, '/');

        // If filename found (i.e. output isn't already a filename but full path), make output be filename
        if (filename)
        {
            size_t size = strlen(++filename);
            memmove(output, filename, size);
            output[size] = 0;
        }
    }
    else
    {
        // Unknown module
        strcpy(output, "unknown");
    }
    return output;
}

/*
 * GetModuleFromAddress
 *      Finds module handle from some address inside it
 */
static HMODULE GetModuleFromAddress(LPVOID address)
{
    HMODULE module;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (char*)address, &module))
        return module;
    return nullptr;
}

/*
*  StackTracer
*      Constructs the tracer, we basically need to initialize the symbol api
*/
StackTracer::StackTracer(const CONTEXT& context)
{
    // Initialise basic values
    memset(&this->frame, 0, sizeof(frame));
    memcpy(&this->context, &context, sizeof(context));

    // Setup the initial frame context
#if !_M_X64
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrPC.Offset = context.Eip;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Esp;
#else
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrPC.Offset = context.Rip;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
#endif
}

/*
 *   StackTracer::Walk
 *      Walks on the stack, each walk is one frame of backtrace
 *      Returns a frame or null if the walk on the park is not possible anymore
 */
StackTracer::Trace* StackTracer::Walk()
{
    if (StackWalk64(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(), GetCurrentThread(),
        &frame, &context, NULL, NULL, NULL, NULL))
    {
        trace.module = GetModuleFromAddress((void*)frame.AddrPC.Offset);
        trace.frame = (void*)frame.AddrFrame.Offset;
        trace.stack = (void*)frame.AddrStack.Offset;
        trace.pc = (void*)frame.AddrPC.Offset;
        trace.ret = (void*)frame.AddrReturn.Offset;
        return &trace;
    }
    return nullptr;
}
