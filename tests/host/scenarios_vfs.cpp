// Virtual file and virtual path API scenarios. Where Win32 semantics are subtle, the same
// operation runs on a real file with identical content so the expectation comes from the OS.
#include "host.hpp"
#include <algorithm>
#include <atomic>
#include <fstream>
#include <sstream>
#include <thread>
#include <filesystem>

using namespace host;

namespace
{
    std::string Pattern(size_t n, uint32_t seed)
    {
        std::string s(n, '\0');
        uint32_t x = seed * 2654435761u + 12345;
        for (size_t i = 0; i < n; ++i)
        {
            x = x * 1103515245u + 12345u;
            s[i] = (char)('A' + (x >> 16) % 26);
        }
        return s;
    }

    HANDLE Open(const std::wstring& p, DWORD access = GENERIC_READ, DWORD disposition = OPEN_EXISTING, DWORD flags = FILE_ATTRIBUTE_NORMAL)
    {
        return CreateFileW(p.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, disposition, flags, nullptr);
    }

    std::string ReadN(HANDLE h, DWORD n, BOOL* okOut = nullptr, DWORD* errOut = nullptr)
    {
        std::string buf(n, '\0');
        DWORD got = 0;
        SetLastError(0);
        BOOL ok = ReadFile(h, buf.data(), n, &got, nullptr);
        if (okOut) *okOut = ok;
        if (errOut) *errOut = GetLastError();
        buf.resize(ok ? got : 0);
        return buf;
    }

    bool AddVirtual(Host& h, const std::wstring& path, const std::string& data, int prio = 1000)
    {
        return h.ual.AddVirtualFileForOverloadW(path.c_str(), (const uint8_t*)data.data(), data.size(), prio);
    }

    // Real file in the game directory, not overloaded
    std::wstring MakeReal(const std::wstring& name, const std::string& data)
    {
        auto p = ExeDir() + L"\\" + name;
        WriteFileW(p, data);
        return p;
    }

    struct SeekResult
    {
        BOOL ok;
        DWORD err;
        long long pos;
        bool operator==(const SeekResult&) const = default;
    };

    SeekResult SeekEx(HANDLE h, long long dist, DWORD method)
    {
        LARGE_INTEGER li, np{};
        li.QuadPart = dist;
        SetLastError(0);
        BOOL ok = SetFilePointerEx(h, li, &np, method);
        return { ok, ok ? 0 : GetLastError(), ok ? np.QuadPart : -1 };
    }

    std::string Describe(const SeekResult& s)
    {
        return "ok=" + std::to_string(s.ok) + " err=" + std::to_string(s.err) + " pos=" + std::to_string(s.pos);
    }
}

// --- virtual files

HOST_SCENARIO(vf_basic)
{
    if (!host.RequireUal()) return;
    const std::string data = "Hello, virtual world!\r\n0123456789";
    host.Check(AddVirtual(host, L"vf_basic.bin", data), "AddVirtualFileForOverloadW returns true");

    HANDLE f = Open(L"vf_basic.bin");
    if (!host.Check(f != INVALID_HANDLE_VALUE, "CreateFileW opens the virtual file", "err=" + std::to_string(GetLastError())))
        return;

    DWORD hi = 0xFFFF;
    host.CheckEq(GetFileSize(f, &hi), (DWORD)data.size(), "GetFileSize");
    host.CheckEq(hi, (DWORD)0, "GetFileSize high part");
    LARGE_INTEGER li{};
    host.Check(GetFileSizeEx(f, &li) && li.QuadPart == (LONGLONG)data.size(), "GetFileSizeEx");
    host.CheckEq(GetFileType(f), (DWORD)FILE_TYPE_DISK, "GetFileType");

    BY_HANDLE_FILE_INFORMATION bhfi{};
    if (host.Check(GetFileInformationByHandle(f, &bhfi) != FALSE, "GetFileInformationByHandle"))
    {
        host.CheckEq(bhfi.nFileSizeLow, (DWORD)data.size(), "BY_HANDLE_FILE_INFORMATION size");
        host.CheckEq(bhfi.nNumberOfLinks, (DWORD)1, "BY_HANDLE_FILE_INFORMATION links");
        host.CheckEq(bhfi.dwFileAttributes, (DWORD)FILE_ATTRIBUTE_NORMAL, "BY_HANDLE_FILE_INFORMATION attributes");
    }
    FILE_STANDARD_INFO fsi{};
    if (host.Check(GetFileInformationByHandleEx(f, FileStandardInfo, &fsi, sizeof(fsi)) != FALSE, "GetFileInformationByHandleEx(FileStandardInfo)"))
    {
        host.CheckEq(fsi.EndOfFile.QuadPart, (LONGLONG)data.size(), "FILE_STANDARD_INFO EndOfFile");
        host.Check(!fsi.Directory, "FILE_STANDARD_INFO Directory is FALSE");
    }
    FILE_BASIC_INFO fbi{};
    if (host.Check(GetFileInformationByHandleEx(f, FileBasicInfo, &fbi, sizeof(fbi)) != FALSE, "GetFileInformationByHandleEx(FileBasicInfo)"))
        host.CheckEq(fbi.FileAttributes, (DWORD)FILE_ATTRIBUTE_NORMAL, "FILE_BASIC_INFO attributes");

    std::string got;
    host.Check(ReadAll(f, got, 7), "ReadFile in small chunks succeeds");
    host.CheckEq(got, data, "content read with ReadFile");
    BOOL ok;
    auto tail = ReadN(f, 16, &ok);
    host.Check(ok && tail.empty(), "ReadFile at EOF returns TRUE with 0 bytes");
    host.Check(CloseHandle(f) != FALSE, "CloseHandle on a virtual handle");

    host.CheckEq(GetFileAttributesW(L"vf_basic.bin"), (DWORD)FILE_ATTRIBUTE_NORMAL, "GetFileAttributesW");
    host.CheckEq(GetFileAttributesA("vf_basic.bin"), (DWORD)FILE_ATTRIBUTE_NORMAL, "GetFileAttributesA");
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    host.Check(GetFileAttributesExW(L"vf_basic.bin", GetFileExInfoStandard, &fad) && fad.nFileSizeLow == data.size(), "GetFileAttributesExW reports the size");
    host.Check(GetFileAttributesExA("vf_basic.bin", GetFileExInfoStandard, &fad) && fad.nFileSizeLow == data.size(), "GetFileAttributesExA reports the size");

    HANDLE fa = CreateFileA("vf_basic.bin", GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (host.Check(fa != INVALID_HANDLE_VALUE, "CreateFileA opens the virtual file"))
    {
        std::string s;
        ReadAll(fa, s);
        host.CheckEq(s, data, "content via CreateFileA");
        CloseHandle(fa);
    }
    HANDLE f2 = CreateFile2(L"vf_basic.bin", GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
    if (host.Check(f2 != INVALID_HANDLE_VALUE, "CreateFile2 opens the virtual file"))
    {
        std::string s;
        ReadAll(f2, s);
        host.CheckEq(s, data, "content via CreateFile2");
        CloseHandle(f2);
    }
    {
        std::ifstream in("vf_basic.bin", std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        host.CheckEq(ss.str(), data, "content via std::ifstream");
    }
    host.CheckEq(ReadFileW(ExeDir() + L"\\vf_basic.bin"), data, "absolute path resolves to the same virtual file");
    host.CheckEq(ReadFileW(L"VF_BASIC.BIN"), data, "lookup is case-insensitive");
    host.CheckEq(ReadFileW(L".\\sub\\..\\vf_basic.bin"), data, "lookup normalizes . and ..");

    host.Check(AddVirtual(host, L"vdir/Nested.Bin", "nested"), "AddVirtualFileForOverloadW with a sub directory");
    host.CheckEq(ReadFileW(L"VDIR\\nested.bin"), std::string("nested"), "forward and back slashes are equivalent");
}

HOST_SCENARIO(vf_append)
{
    if (!host.RequireUal()) return;
    // From the original tests.cpp: adding to the same path appends
    host.Check(AddVirtual(host, L"vf_append.bin", "virtual_file_", 1000), "first add");
    host.Check(AddVirtual(host, L"vf_append.bin", "test_passed.txt", 1000), "second add with equal priority");
    host.CheckEq(ReadFileW(L"vf_append.bin"), std::string("virtual_file_test_passed.txt"), "data of equal priority adds is appended");

    host.Check(!AddVirtual(host, L"vf_append.bin", "LOW", 999), "an add with lower priority is rejected");
    host.CheckEq(ReadFileW(L"vf_append.bin"), std::string("virtual_file_test_passed.txt"), "lower priority add leaves the content untouched");

    host.Check(AddVirtual(host, L"vf_append.bin", "!", 1001), "an add with higher priority is accepted");
    host.CheckEq(ReadFileW(L"vf_append.bin"), std::string("virtual_file_test_passed.txt!"), "a higher priority add appends as well (current contract)");
    host.Check(!AddVirtual(host, L"vf_append.bin", "?", 1000), "after a higher priority add, the old priority is rejected");

    std::string a = "ansi";
    host.Check(host.ual.AddVirtualFileForOverloadA("vf_append_a.bin", (const uint8_t*)a.data(), a.size(), 1000), "AddVirtualFileForOverloadA");
    host.CheckEq(ReadFileW(L"vf_append_a.bin"), a, "file added with the A variant is visible through the W API");
}

HOST_SCENARIO(vf_access)
{
    if (!host.RequireUal()) return;
    AddVirtual(host, L"vf_access.bin", "readonly");

    SetLastError(0);
    HANDLE w = Open(L"vf_access.bin", GENERIC_WRITE);
    host.Check(w == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED, "GENERIC_WRITE is denied", "err=" + std::to_string(GetLastError()));
    if (w != INVALID_HANDLE_VALUE) CloseHandle(w);

    HANDLE rw = Open(L"vf_access.bin", GENERIC_READ | GENERIC_WRITE);
    host.Check(rw == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED, "GENERIC_READ|GENERIC_WRITE is denied");
    if (rw != INVALID_HANDLE_VALUE) CloseHandle(rw);

    HANDLE cn = Open(L"vf_access.bin", GENERIC_READ, CREATE_NEW);
    host.Check(cn == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_EXISTS, "CREATE_NEW fails with ERROR_FILE_EXISTS");
    if (cn != INVALID_HANDLE_VALUE) CloseHandle(cn);

    // Behaves like a read-only file
    HANDLE ca = Open(L"vf_access.bin", GENERIC_READ, CREATE_ALWAYS);
    host.Check(ca == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED, "CREATE_ALWAYS fails with ERROR_ACCESS_DENIED", "err=" + std::to_string(GetLastError()));
    if (ca != INVALID_HANDLE_VALUE) CloseHandle(ca);

    HANDLE oa = Open(L"vf_access.bin", GENERIC_READ, OPEN_ALWAYS);
    if (host.Check(oa != INVALID_HANDLE_VALUE, "OPEN_ALWAYS with read access succeeds"))
    {
        std::string s;
        ReadAll(oa, s);
        host.CheckEq(s, std::string("readonly"), "OPEN_ALWAYS reads the virtual content");
        CloseHandle(oa);
    }
}

// only GENERIC_WRITE is rejected. Other write rights silently get a read-only handle.
HOST_SCENARIO(vf_access_write_rights)
{
    if (!host.RequireUal()) return;
    AddVirtual(host, L"vf_rights.bin", "readonly");
    struct { DWORD access; const char* name; } cases[] = {
        { GENERIC_ALL, "GENERIC_ALL" },
        { FILE_WRITE_DATA, "FILE_WRITE_DATA" },
        { FILE_APPEND_DATA, "FILE_APPEND_DATA" },
        { FILE_GENERIC_WRITE, "FILE_GENERIC_WRITE" },
    };
    for (auto& c : cases)
    {
        SetLastError(0);
        HANDLE f = Open(L"vf_rights.bin", c.access);
        DWORD err = GetLastError();
        host.Check(f == INVALID_HANDLE_VALUE && err == ERROR_ACCESS_DENIED, std::string(c.name) + " is denied on a read-only virtual file",
            f == INVALID_HANDLE_VALUE ? "err=" + std::to_string(err) : "a handle was returned");
        if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    }
}

HOST_SCENARIO(vf_seek)
{
    if (!host.RequireUal()) return;
    const std::string data = Pattern(100, 7);
    AddVirtual(host, L"vf_seek.bin", data);
    auto realPath = MakeReal(L"real_seek.bin", data);
    HANDLE v = Open(L"vf_seek.bin");
    HANDLE r = Open(realPath);
    if (!host.Check(v != INVALID_HANDLE_VALUE && r != INVALID_HANDLE_VALUE, "open virtual and real file")) return;

    auto both = [&](const char* name, auto op) {
        auto a = op(v), b = op(r);
        host.Check(a == b, std::string("virtual matches real: ") + name, "virtual " + Describe(a) + " / real " + Describe(b));
        return a;
    };

    host.CheckEq(both("SetFilePointerEx BEGIN 10", [](HANDLE h) { return SeekEx(h, 10, FILE_BEGIN); }).pos, 10LL, "seek to 10");
    host.CheckEq(ReadN(v, 5), data.substr(10, 5), "read after FILE_BEGIN seek");
    ReadN(r, 5);
    both("SetFilePointerEx CURRENT -5", [](HANDLE h) { return SeekEx(h, -5, FILE_CURRENT); });
    host.CheckEq(ReadN(v, 3), data.substr(10, 3), "read after FILE_CURRENT seek");
    ReadN(r, 3);
    both("SetFilePointerEx END -10", [](HANDLE h) { return SeekEx(h, -10, FILE_END); });
    host.CheckEq(ReadN(v, 20), data.substr(90), "short read at the end of the file");
    ReadN(r, 20);
    both("SetFilePointerEx CURRENT 0 at EOF", [](HANDLE h) { return SeekEx(h, 0, FILE_CURRENT); });
    both("SetFilePointerEx END 0", [](HANDLE h) { return SeekEx(h, 0, FILE_END); });
    both("SetFilePointerEx BEGIN -1", [](HANDLE h) { return SeekEx(h, -1, FILE_BEGIN); });
    both("SetFilePointerEx CURRENT to before start", [](HANDLE h) { SeekEx(h, 3, FILE_BEGIN); return SeekEx(h, -4, FILE_CURRENT); });

    auto sfp = [](HANDLE h, LONG lo, LONG* hi, DWORD m) {
        SetLastError(0);
        DWORD res = SetFilePointer(h, lo, hi, m);
        DWORD err = GetLastError();
        return SeekResult{ res != INVALID_SET_FILE_POINTER || err == NO_ERROR, res == INVALID_SET_FILE_POINTER ? err : 0, (long long)res };
    };
    both("SetFilePointer BEGIN 20 (no high part)", [&](HANDLE h) { return sfp(h, 20, nullptr, FILE_BEGIN); });
    both("SetFilePointer BEGIN 30 with high part 0", [&](HANDLE h) { LONG hi = 0; return sfp(h, 30, &hi, FILE_BEGIN); });
    both("SetFilePointer CURRENT -5 with high part -1", [&](HANDLE h) { LONG hi = -1; return sfp(h, -5, &hi, FILE_CURRENT); });
    host.CheckEq(ReadN(v, 4), data.substr(25, 4), "read after 64-bit negative SetFilePointer");
    ReadN(r, 4);

    LARGE_INTEGER zero{};
    SetLastError(0);
    host.Check(!SetFilePointerEx(v, zero, nullptr, 7) && GetLastError() == ERROR_INVALID_PARAMETER, "invalid move method fails with ERROR_INVALID_PARAMETER");
    CloseHandle(v);
    CloseHandle(r);
}

// SetFilePointer without lpDistanceToMoveHigh must sign-extend the distance.
// The loader zero-extends it, so negative seeks fail.
HOST_SCENARIO(vf_seek_negative_low)
{
    if (!host.RequireUal()) return;
    const std::string data = Pattern(100, 9);
    AddVirtual(host, L"vf_neg.bin", data);
    auto realPath = MakeReal(L"real_neg.bin", data);
    HANDLE v = Open(L"vf_neg.bin"), r = Open(realPath);
    if (!host.Check(v != INVALID_HANDLE_VALUE && r != INVALID_HANDLE_VALUE, "open virtual and real file")) return;

    DWORD rv = SetFilePointer(r, -10, nullptr, FILE_END);
    DWORD vv = SetFilePointer(v, -10, nullptr, FILE_END);
    host.CheckEq(rv, (DWORD)90, "real file: SetFilePointer(-10, NULL, FILE_END)");
    host.CheckEq(vv, rv, "virtual file: SetFilePointer(-10, NULL, FILE_END)");

    SetFilePointer(r, 50, nullptr, FILE_BEGIN);
    SetFilePointer(v, 50, nullptr, FILE_BEGIN);
    rv = SetFilePointer(r, -20, nullptr, FILE_CURRENT);
    vv = SetFilePointer(v, -20, nullptr, FILE_CURRENT);
    host.CheckEq(rv, (DWORD)30, "real file: SetFilePointer(-20, NULL, FILE_CURRENT)");
    host.CheckEq(vv, rv, "virtual file: SetFilePointer(-20, NULL, FILE_CURRENT)");
    CloseHandle(v);
    CloseHandle(r);
}

// seeking past EOF is legal on Windows and later reads return 0 bytes.
// The loader fails it with ERROR_NEGATIVE_SEEK or ERROR_SEEK_ON_DEVICE.
HOST_SCENARIO(vf_seek_past_eof)
{
    if (!host.RequireUal()) return;
    const std::string data = Pattern(64, 3);
    AddVirtual(host, L"vf_past.bin", data);
    auto realPath = MakeReal(L"real_past.bin", data);
    HANDLE v = Open(L"vf_past.bin"), r = Open(realPath);
    if (!host.Check(v != INVALID_HANDLE_VALUE && r != INVALID_HANDLE_VALUE, "open virtual and real file")) return;

    auto check = [&](const char* name, long long dist, DWORD method) {
        auto a = SeekEx(v, dist, method), b = SeekEx(r, dist, method);
        host.Check(b.ok, std::string("real file: ") + name + " succeeds", Describe(b));
        host.Check(a == b, std::string("virtual matches real: ") + name, "virtual " + Describe(a) + " / real " + Describe(b));
    };
    check("FILE_BEGIN size+10", 74, FILE_BEGIN);
    BOOL okV, okR;
    auto sv = ReadN(v, 8, &okV);
    auto sr = ReadN(r, 8, &okR);
    host.Check(okV == okR && sv == sr, "read past EOF behaves like a real file");
    check("FILE_END +5", 5, FILE_END);
    check("FILE_CURRENT +100", 100, FILE_CURRENT);
    CloseHandle(v);
    CloseHandle(r);
}

// the file position is per virtual file, not per handle, and every CreateFile
// on the same path rewinds all open handles.
HOST_SCENARIO(vf_two_handles)
{
    if (!host.RequireUal()) return;
    const std::string data = "0123456789ABCDEF";
    AddVirtual(host, L"vf_two.bin", data);
    auto realPath = MakeReal(L"real_two.bin", data);

    auto run = [&](const std::wstring& path, const char* label) {
        HANDLE h1 = Open(path);
        auto a = ReadN(h1, 4);
        HANDLE h2 = Open(path);
        auto b = ReadN(h1, 4);
        auto c = ReadN(h2, 4);
        auto d = ReadN(h1, 4);
        CloseHandle(h1);
        CloseHandle(h2);
        host.CheckEq(a, std::string("0123"), std::string(label) + ": first handle, first read");
        host.CheckEq(b, std::string("4567"), std::string(label) + ": opening a second handle does not rewind the first");
        host.CheckEq(c, std::string("0123"), std::string(label) + ": second handle starts at offset 0");
        host.CheckEq(d, std::string("89AB"), std::string(label) + ": reads through the second handle do not move the first");
    };
    run(realPath, "real file");
    run(L"vf_two.bin", "virtual file");
}

HOST_SCENARIO(vf_overlapped)
{
    if (!host.RequireUal()) return;
    const std::string data = Pattern(64, 11);
    AddVirtual(host, L"vf_ov.bin", data);
    HANDLE v = Open(L"vf_ov.bin");
    if (!host.Check(v != INVALID_HANDLE_VALUE, "open virtual file")) return;

    OVERLAPPED ov{};
    ov.Offset = 10;
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    char buf[8] = {};
    DWORD n = 0;
    host.Check(ReadFile(v, buf, 5, &n, &ov) != FALSE, "overlapped ReadFile succeeds");
    host.CheckEq(n, (DWORD)5, "bytes read");
    host.CheckEq(std::string(buf, 5), data.substr(10, 5), "data at the requested offset");
    host.CheckEq(WaitForSingleObject(ov.hEvent, 0), (DWORD)WAIT_OBJECT_0, "event is signaled");
    host.CheckEq((DWORD)ov.InternalHigh, (DWORD)5, "OVERLAPPED.InternalHigh holds the byte count");
    DWORD n2 = 0;
    host.Check(GetOverlappedResult(v, &ov, &n2, FALSE) && n2 == 5, "GetOverlappedResult reports the transfer");

    ov.Offset = 60;
    ResetEvent(ov.hEvent);
    host.Check(ReadFile(v, buf, 8, &n, &ov) && n == 4, "overlapped read crossing EOF returns the remaining bytes");
    CloseHandle(ov.hEvent);
    CloseHandle(v);
}

// an overlapped read at or past EOF must fail with ERROR_HANDLE_EOF like a real file.
// The loader reports success.
HOST_SCENARIO(vf_overlapped_eof)
{
    if (!host.RequireUal()) return;
    const std::string data = Pattern(32, 5);
    AddVirtual(host, L"vf_oveof.bin", data);
    auto realPath = MakeReal(L"real_oveof.bin", data);
    auto attempt = [&](const std::wstring& p) {
        HANDLE h = Open(p);
        OVERLAPPED ov{};
        ov.Offset = 40;
        char buf[8];
        DWORD n = 99;
        SetLastError(0);
        BOOL ok = ReadFile(h, buf, 8, &n, &ov);
        DWORD err = GetLastError();
        CloseHandle(h);
        return std::make_pair(ok, ok ? 0 : err);
    };
    auto real = attempt(realPath);
    auto virt = attempt(L"vf_oveof.bin");
    host.Check(real.first == FALSE && real.second == ERROR_HANDLE_EOF, "real file: overlapped read beyond EOF fails with ERROR_HANDLE_EOF",
        "ok=" + std::to_string(real.first) + " err=" + std::to_string(real.second));
    host.Check(virt == real, "virtual file: overlapped read beyond EOF matches the real file",
        "ok=" + std::to_string(virt.first) + " err=" + std::to_string(virt.second));
}

namespace
{
    std::atomic<DWORD> g_cbThread{ 0 };
    std::atomic<int> g_cbCount{ 0 };
    std::atomic<DWORD> g_cbBytes{ 0 };
    VOID CALLBACK ReadExDone(DWORD, DWORD n, LPOVERLAPPED)
    {
        g_cbThread = GetCurrentThreadId();
        g_cbBytes = n;
        ++g_cbCount;
    }
}

// ReadFileEx completion routines are APCs and must run on the calling thread during
// an alertable wait. The loader runs them at once on a thread-pool thread.
HOST_SCENARIO(vf_readfileex)
{
    if (!host.RequireUal()) return;
    const std::string data = Pattern(32, 21);
    AddVirtual(host, L"vf_rfex.bin", data);
    auto realPath = MakeReal(L"real_rfex.bin", data);

    auto run = [&](const std::wstring& p, const std::string& label) {
        g_cbCount = 0;
        g_cbThread = 0;
        g_cbBytes = 0;
        HANDLE h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        if (!host.Check(h != INVALID_HANDLE_VALUE, label + ": open with FILE_FLAG_OVERLAPPED")) return;
        OVERLAPPED ov{};
        char buf[16];
        host.Check(ReadFileEx(h, buf, 8, &ov, ReadExDone) != FALSE, label + ": ReadFileEx succeeds");
        WaitForSingleObject(GetCurrentProcess(), 300); // non-alertable, the APC must not run
        host.CheckEq(g_cbCount.load(), 0, label + ": completion routine does not run outside an alertable wait");
        DWORD w = SleepEx(3000, TRUE);
        host.CheckEq(w, (DWORD)WAIT_IO_COMPLETION, label + ": alertable SleepEx returns WAIT_IO_COMPLETION");
        host.CheckEq(g_cbCount.load(), 1, label + ": completion routine ran exactly once");
        host.CheckEq(g_cbThread.load(), GetCurrentThreadId(), label + ": completion routine ran on the calling thread");
        host.CheckEq(g_cbBytes.load(), (DWORD)8, label + ": completion routine got the byte count");
        CloseHandle(h);
    };
    run(realPath, "real file");
    run(L"vf_rfex.bin", "virtual file");
}

// CreateFileMapping on a virtual file honours the requested protection like on a real file
HOST_SCENARIO(vf_mapping_protection)
{
    if (!host.RequireUal()) return;
    const std::string data = Pattern(4096, 7);
    AddVirtual(host, L"vf_map.bin", data);
    auto realPath = MakeReal(L"real_map.bin", data);

    auto run = [&](const std::wstring& p, const std::string& label) {
        HANDLE h = CreateFileW(p.c_str(), GENERIC_READ | GENERIC_EXECUTE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (!host.Check(h != INVALID_HANDLE_VALUE, label + ": open for read and execute")) return;

        HANDLE ro = CreateFileMappingW(h, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (host.Check(ro != nullptr, label + ": PAGE_READONLY mapping"))
        {
            void* view = MapViewOfFile(ro, FILE_MAP_READ, 0, 0, 0);
            if (host.Check(view != nullptr, label + ": read view of a read-only mapping"))
            {
                host.Check(memcmp(view, data.data(), data.size()) == 0, label + ": the view holds the file contents");
                UnmapViewOfFile(view);
            }
            void* w = MapViewOfFile(ro, FILE_MAP_WRITE, 0, 0, 0);
            host.Check(w == nullptr && GetLastError() == ERROR_ACCESS_DENIED, label + ": no writable view of a read-only mapping");
            if (w) UnmapViewOfFile(w);
            void* x = MapViewOfFile(ro, FILE_MAP_READ | FILE_MAP_EXECUTE, 0, 0, 0);
            host.Check(x == nullptr && GetLastError() == ERROR_ACCESS_DENIED, label + ": no executable view of a read-only mapping");
            if (x) UnmapViewOfFile(x);
            CloseHandle(ro);
        }

        HANDLE rx = CreateFileMappingW(h, nullptr, PAGE_EXECUTE_READ, 0, 0, nullptr);
        if (host.Check(rx != nullptr, label + ": PAGE_EXECUTE_READ mapping"))
        {
            void* x = MapViewOfFile(rx, FILE_MAP_READ | FILE_MAP_EXECUTE, 0, 0, 0);
            if (host.Check(x != nullptr, label + ": executable view of an execute-read mapping"))
            {
                MEMORY_BASIC_INFORMATION mbi{};
                VirtualQuery(x, &mbi, sizeof(mbi));
                host.CheckEq(mbi.Protect, (DWORD)PAGE_EXECUTE_READ, label + ": the view is PAGE_EXECUTE_READ");
                UnmapViewOfFile(x);
            }
            void* w = MapViewOfFile(rx, FILE_MAP_WRITE, 0, 0, 0);
            host.Check(w == nullptr && GetLastError() == ERROR_ACCESS_DENIED, label + ": no writable view of an execute-read mapping");
            if (w) UnmapViewOfFile(w);
            CloseHandle(rx);
        }
        CloseHandle(h);
    };
    run(realPath, "real file");
    run(L"vf_map.bin", "virtual file");
}

HOST_SCENARIO(vf_remove)
{
    if (!host.RequireUal()) return;
    AddVirtual(host, L"vf_rm.bin", "gone soon");
    host.CheckEq(ReadFileW(L"vf_rm.bin"), std::string("gone soon"), "file is readable before removal");
    HANDLE open = Open(L"vf_rm.bin");
    host.ual.RemoveVirtualFileFromOverloadW(L"vf_rm.bin");
    DWORD err = 0;
    ReadFileW(L"vf_rm.bin", &err);
    host.CheckEq(err, (DWORD)ERROR_FILE_NOT_FOUND, "CreateFileW fails with ERROR_FILE_NOT_FOUND after removal");
    host.CheckEq(GetFileAttributesW(L"vf_rm.bin"), (DWORD)INVALID_FILE_ATTRIBUTES, "GetFileAttributesW reports no file after removal");
    host.Check(CloseHandle(open) != FALSE, "a handle opened before removal can still be closed");

    std::string a = "ansi";
    host.ual.AddVirtualFileForOverloadA("vf_rm_a.bin", (const uint8_t*)a.data(), a.size(), 1000);
    host.CheckEq(ReadFileW(L"vf_rm_a.bin"), a, "A variant: readable before removal");
    host.ual.RemoveVirtualFileFromOverloadA("vf_rm_a.bin");
    ReadFileW(L"vf_rm_a.bin", &err);
    host.CheckEq(err, (DWORD)ERROR_FILE_NOT_FOUND, "A variant: removed");

    host.ual.RemoveVirtualFileFromOverloadW(L"never_added.bin");
    host.ual.RemoveVirtualFileFromOverloadW(nullptr);
    host.ual.RemoveVirtualFileFromOverloadA(nullptr);
    host.Check(true, "removing unknown or null paths is harmless");
}

HOST_SCENARIO(vf_shadow_real)
{
    if (!host.RequireUal()) return;
    MakeReal(L"shadow.txt", "REAL");
    host.CheckEq(ReadFileW(L"shadow.txt"), std::string("REAL"), "real file before registering a virtual one");
    AddVirtual(host, L"shadow.txt", "VIRTUAL");
    host.CheckEq(ReadFileW(L"shadow.txt"), std::string("VIRTUAL"), "virtual file shadows the real file");
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    host.Check(GetFileAttributesExW(L"shadow.txt", GetFileExInfoStandard, &fad) && fad.nFileSizeLow == 7, "attributes come from the virtual file");
    host.ual.RemoveVirtualFileFromOverloadW(L"shadow.txt");
    host.CheckEq(ReadFileW(L"shadow.txt"), std::string("REAL"), "real file is visible again after removal");
}

HOST_SCENARIO(vf_invalid_args)
{
    if (!host.RequireUal()) return;
    const uint8_t d[] = { 1, 2, 3 };
    host.Check(!host.ual.AddVirtualFileForOverloadW(nullptr, d, 3, 1000), "null path is rejected (W)");
    host.Check(!host.ual.AddVirtualFileForOverloadA(nullptr, d, 3, 1000), "null path is rejected (A)");
    host.Check(!host.ual.AddVirtualFileForOverloadW(L"x.bin", nullptr, 3, 1000), "null data is rejected");
    host.Check(!host.ual.AddVirtualFileForOverloadW(L"x.bin", d, 0, 1000), "empty data is rejected");
    host.Check(!host.ual.AddVirtualPathForOverloadW(nullptr, L"y", 1000), "null original path is rejected");
    host.Check(!host.ual.AddVirtualPathForOverloadW(L"y", nullptr, 1000), "null virtual path is rejected");
    host.Check(!host.ual.AddVirtualPathForOverloadA(nullptr, "y", 1000), "null original path is rejected (A)");
    host.ual.RemoveVirtualPathFromOverloadW(nullptr);
    host.ual.RemoveVirtualPathFromOverloadA(nullptr);
    host.Check(true, "removing null virtual paths is harmless");
}

HOST_SCENARIO(vf_large)
{
    if (!host.RequireUal()) return;
    const size_t size = 8 * 1024 * 1024 + 123;
    const std::string data = Pattern(size, 77);
    host.Check(AddVirtual(host, L"vf_large.bin", data), "add 8 MiB virtual file");
    HANDLE v = Open(L"vf_large.bin");
    if (!host.Check(v != INVALID_HANDLE_VALUE, "open")) return;
    std::string got;
    host.Check(ReadAll(v, got, 65536), "chunked read");
    host.Check(got == data, "8 MiB content round-trips", "size " + std::to_string(got.size()));

    uint32_t x = 1;
    bool allOk = true;
    for (int i = 0; i < 200 && allOk; ++i)
    {
        x = x * 1664525u + 1013904223u;
        size_t off = x % size;
        size_t len = 1 + (x >> 8) % 70000;
        LARGE_INTEGER li;
        li.QuadPart = (LONGLONG)off;
        SetFilePointerEx(v, li, nullptr, FILE_BEGIN);
        auto s = ReadN(v, (DWORD)len);
        allOk = s == data.substr(off, len);
    }
    host.Check(allOk, "200 random seek+read operations return the right bytes");
    CloseHandle(v);
}

// FindFirstFile misses virtual files that CreateFile and GetFileAttributes see,
// so games probing with FindFirstFile don't find them.
HOST_SCENARIO(vf_findfirst)
{
    if (!host.RequireUal()) return;
    const std::string data = "findable";
    AddVirtual(host, L"vf_find.bin", data);
    host.CheckEq(GetFileAttributesW(L"vf_find.bin"), (DWORD)FILE_ATTRIBUTE_NORMAL, "GetFileAttributesW sees the virtual file");
    WIN32_FIND_DATAW fd{};
    HANDLE f = FindFirstFileW(L"vf_find.bin", &fd);
    host.Check(f != INVALID_HANDLE_VALUE, "FindFirstFileW sees the virtual file", "err=" + std::to_string(GetLastError()));
    if (f != INVALID_HANDLE_VALUE)
    {
        host.CheckEq(fd.nFileSizeLow, (DWORD)data.size(), "FindFirstFileW reports the virtual size");
        FindClose(f);
    }
}

HOST_SCENARIO(vf_threads)
{
    if (!host.RequireUal()) return;
    constexpr int kThreads = 8;
    std::atomic<int> failures{ 0 };
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&, t] {
            auto name = L"vf_thr_" + std::to_wstring(t) + L".bin";
            auto data = Pattern(256 * 1024, 100 + t);
            if (!host.ual.AddVirtualFileForOverloadW(name.c_str(), (const uint8_t*)data.data(), data.size(), 1000)) { ++failures; return; }
            for (int i = 0; i < 40; ++i)
                if (ReadFileW(name) != data) { ++failures; return; }
        });
    }
    for (auto& t : threads) t.join();
    host.CheckEq(failures.load(), 0, "8 threads reading their own virtual files concurrently");
}

// ReadVirtualFile copies from a VirtualFile* after releasing the lock while other
// threads remove (free) or append to (reallocate) it. Large buffers go back to the OS on free,
// so a read racing a removal faults.
HOST_SCENARIO(vf_storage_race)
{
    if (!host.RequireUal()) return;
    const std::string big(32 * 1024 * 1024, 'R');
    AddVirtual(host, L"vf_race.bin", big);
    std::atomic<bool> stop{ false };
    std::atomic<int> bad{ 0 }, reads{ 0 }, cycles{ 0 };
    std::thread writer([&] {
        auto until = GetTickCount64() + 4000;
        while (GetTickCount64() < until)
        {
            host.ual.RemoveVirtualFileFromOverloadW(L"vf_race.bin");
            host.ual.AddVirtualFileForOverloadW(L"vf_race.bin", (const uint8_t*)big.data(), big.size(), 1000);
            host.ual.AddVirtualFileForOverloadW(L"vf_race.bin", (const uint8_t*)"R", 1, 1000); // append reallocates
            ++cycles;
        }
        stop = true;
    });
    std::vector<std::thread> readers;
    for (int t = 0; t < 3; ++t)
        readers.emplace_back([&] {
            std::string buf(big.size() + 64, '\0');
            while (!stop)
            {
                HANDLE h = CreateFileW(L"vf_race.bin", GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
                if (h == INVALID_HANDLE_VALUE) continue; // just removed
                DWORD n = 0;
                std::fill(buf.begin(), buf.begin() + 4096, '\0');
                if (ReadFile(h, buf.data(), (DWORD)buf.size(), &n, nullptr) && n > 0)
                {
                    ++reads;
                    if (buf.find_first_not_of('R', 0) < n) ++bad;
                }
                CloseHandle(h);
            }
        });
    writer.join();
    for (auto& r : readers) r.join();
    host.CheckEq(bad.load(), 0, "every read returns consistent data while the file is removed, re-added and appended concurrently (" +
        std::to_string(reads.load()) + " reads, " + std::to_string(cycles.load()) + " writer cycles)");
}

// --- virtual paths

HOST_SCENARIO(vpath_basic)
{
    if (!host.RequireUal()) return;
    MakeReal(L"vp_orig.bin", "ORIGINAL");
    CreateDirectoryW((ExeDir() + L"\\storage").c_str(), nullptr);
    MakeReal(L"storage\\vp_target.bin", "TARGET");

    host.Check(host.ual.AddVirtualPathForOverloadW(L"vp_orig.bin", L"storage/vp_target.bin", 1000), "AddVirtualPathForOverloadW");
    host.CheckEq(ReadFileW(L"vp_orig.bin"), std::string("TARGET"), "mapped path is redirected");
    host.CheckEq(ReadFileW(L"VP_ORIG.BIN"), std::string("TARGET"), "mapping is case-insensitive");
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    host.Check(GetFileAttributesExW(L"vp_orig.bin", GetFileExInfoStandard, &fad) && fad.nFileSizeLow == 6, "GetFileAttributesExW follows the mapping");
    host.ual.RemoveVirtualPathFromOverloadW(L"vp_orig.bin");
    host.CheckEq(ReadFileW(L"vp_orig.bin"), std::string("ORIGINAL"), "removing the mapping restores the original");

    host.Check(host.ual.AddVirtualPathForOverloadA("vp_orig.bin", "storage\\vp_target.bin", 1000), "AddVirtualPathForOverloadA");
    host.CheckEq(ReadFileW(L"vp_orig.bin"), std::string("TARGET"), "A variant redirects");
    host.ual.RemoveVirtualPathFromOverloadA("vp_orig.bin");
    host.CheckEq(ReadFileW(L"vp_orig.bin"), std::string("ORIGINAL"), "A variant removal");

    host.Check(host.ual.AddVirtualPathForOverloadW(L"vp_new_name.bin", L"storage\\vp_target.bin", 1000), "map a name that does not exist on disk");
    host.CheckEq(ReadFileW(L"vp_new_name.bin"), std::string("TARGET"), "a mapping can introduce a new file name");
    DWORD err = 0;
    host.ual.AddVirtualPathForOverloadW(L"vp_dangling.bin", L"storage\\does_not_exist.bin", 1000);
    ReadFileW(L"vp_dangling.bin", &err);
    host.Check(err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND, "a mapping to a missing file fails to open", "err=" + std::to_string(err));
}

HOST_SCENARIO(vpath_priority)
{
    if (!host.RequireUal()) return;
    CreateDirectoryW((ExeDir() + L"\\storage").c_str(), nullptr);
    MakeReal(L"storage\\p1.bin", "ONE");
    MakeReal(L"storage\\p2.bin", "TWO");
    host.Check(host.ual.AddVirtualPathForOverloadW(L"vp_prio.bin", L"storage\\p1.bin", 1000), "first mapping");
    host.Check(!host.ual.AddVirtualPathForOverloadW(L"vp_prio.bin", L"storage\\p2.bin", 999), "lower priority mapping is rejected");
    host.Check(!host.ual.AddVirtualPathForOverloadW(L"vp_prio.bin", L"storage\\p2.bin", 1000), "equal priority mapping is rejected");
    host.CheckEq(ReadFileW(L"vp_prio.bin"), std::string("ONE"), "first mapping still active");
    host.Check(host.ual.AddVirtualPathForOverloadW(L"vp_prio.bin", L"storage\\p2.bin", 1001), "higher priority mapping replaces it");
    host.CheckEq(ReadFileW(L"vp_prio.bin"), std::string("TWO"), "new mapping active");
}

HOST_SCENARIO(vpath_vs_vfile)
{
    if (!host.RequireUal()) return;
    CreateDirectoryW((ExeDir() + L"\\storage").c_str(), nullptr);
    MakeReal(L"storage\\pv_target.bin", "MAPPED");
    AddVirtual(host, L"pv.bin", "VFILE", 1000);
    host.CheckEq(ReadFileW(L"pv.bin"), std::string("VFILE"), "virtual file active");
    host.Check(host.ual.AddVirtualPathForOverloadW(L"pv.bin", L"storage\\pv_target.bin", 1001), "higher priority virtual path");
    host.CheckEq(ReadFileW(L"pv.bin"), std::string("MAPPED"), "a higher priority virtual path replaces the virtual file");
    host.Check(AddVirtual(host, L"pv.bin", "VFILE2", 1002), "higher priority virtual file");
    host.CheckEq(ReadFileW(L"pv.bin"), std::string("VFILE2"), "a higher priority virtual file replaces the virtual path");
    host.ual.AddVirtualPathForOverloadW(L"pv.bin", L"storage\\pv_target.bin", 999);
    host.CheckEq(ReadFileW(L"pv.bin"), std::string("VFILE2"), "a lower priority virtual path does not hide the virtual file");
}

HOST_SCENARIO(vpath_chain)
{
    if (!host.RequireUal()) return;
    MakeReal(L"chain_a.bin", "A");
    MakeReal(L"chain_b.bin", "B");
    MakeReal(L"chain_c.bin", "C");
    host.ual.AddVirtualPathForOverloadW(L"chain_a.bin", L"chain_b.bin", 1000);
    host.ual.AddVirtualPathForOverloadW(L"chain_b.bin", L"chain_c.bin", 1000);
    host.CheckEq(ReadFileW(L"chain_a.bin"), std::string("C"), "a -> b -> c resolves to c");
    host.CheckEq(ReadFileW(L"chain_b.bin"), std::string("C"), "b -> c resolves to c");

    MakeReal(L"cycle_a.bin", "CA");
    MakeReal(L"cycle_b.bin", "CB");
    host.ual.AddVirtualPathForOverloadW(L"cycle_a.bin", L"cycle_b.bin", 1000);
    host.ual.AddVirtualPathForOverloadW(L"cycle_b.bin", L"cycle_a.bin", 1000);
    auto s = ReadFileW(L"cycle_a.bin");
    host.Check(s == "CA" || s == "CB", "a mapping cycle terminates and opens one of the files", "got \"" + s + "\"");
}

HOST_SCENARIO(vpath_absolute_target)
{
    if (!host.RequireUal()) return;
    auto outside = std::filesystem::path(ExeDir()).parent_path() / L"outside_target.bin";
    WriteFileW(outside.wstring(), "OUTSIDE");
    host.Check(host.ual.AddVirtualPathForOverloadW(L"vp_abs.bin", outside.c_str(), 1000), "map to an absolute path outside the game directory");
    host.CheckEq(ReadFileW(L"vp_abs.bin"), std::string("OUTSIDE"), "absolute targets are honoured");
}

// GetOverloadedFilePath takes virtualPathMutex shared and recurses into code that
// takes it again. SRW locks aren't re-entrant, so a waiting writer deadlocks it.
HOST_SCENARIO(vpath_deadlock)
{
    if (!host.RequireUal()) return;
    MakeReal(L"dl_a.bin", "A");
    MakeReal(L"dl_b.bin", "B");
    MakeReal(L"dl_c.bin", "C");
    host.ual.AddVirtualPathForOverloadW(L"dl_a.bin", L"dl_b.bin", 1000);
    host.ual.AddVirtualPathForOverloadW(L"dl_b.bin", L"dl_c.bin", 1000);

    std::atomic<bool> stop{ false };
    std::atomic<long> readerIterations{ 0 }, writerIterations{ 0 };
    auto deadline = GetTickCount64() + 3000;
    std::vector<HANDLE> threads;
    struct Ctx { std::atomic<bool>* stop; std::atomic<long>* it; const host::UalApi* api; } rctx{ &stop, &readerIterations, &host.ual }, wctx{ &stop, &writerIterations, &host.ual };
    for (int i = 0; i < 4; ++i)
        threads.push_back(CreateThread(nullptr, 0, [](LPVOID p) -> DWORD {
            auto c = static_cast<Ctx*>(p);
            while (!*c->stop) { DWORD a = GetFileAttributesW(L"dl_a.bin"); (void)a; ++*c->it; }
            return 0;
        }, &rctx, 0, nullptr));
    threads.push_back(CreateThread(nullptr, 0, [](LPVOID p) -> DWORD {
        auto c = static_cast<Ctx*>(p);
        while (!*c->stop)
        {
            c->api->AddVirtualPathForOverloadW(L"dl_x.bin", L"dl_c.bin", 1000);
            c->api->RemoveVirtualPathFromOverloadW(L"dl_x.bin");
            ++*c->it;
        }
        return 0;
    }, &wctx, 0, nullptr));

    while (GetTickCount64() < deadline) WaitForSingleObject(GetCurrentProcess(), 50);
    stop = true;
    DWORD w = WaitForMultipleObjects((DWORD)threads.size(), threads.data(), TRUE, 8000);
    if (w == WAIT_TIMEOUT)
    {
        // Deadlocked. Any file API call, even writing the report, would block, so exit with a marker code.
        TerminateProcess(GetCurrentProcess(), kDeadlockExitCode);
    }
    host.Check(true, "readers and a writer of virtual paths make progress without deadlocking",
        "reader iterations " + std::to_string(readerIterations.load()) + ", writer iterations " + std::to_string(writerIterations.load()));
    for (auto t : threads) CloseHandle(t);
}

// --- update folder scanning (GTA IV FusionFix)
//
// FusionFix gets the update folder from GetOverloadPathW, walks it with
// recursive_directory_iterator, turns each "<name>.img" folder into an archive (merged with
// the game's <name>.img if any), adds it as a virtual file and lets the game read "update\...".
// Must work for an update folder on disk or in a zip. Fixture (tests_zip.cpp):
//   game:    pc\data\gtxd.img = "ORIGINAL-IMG"
//   update:  pc\data\gtxd.img\a.wtd = "A-WTD", pc\data\gtxd.img\sub\b.wtd = "B-WTD",
//            common\new.img\c.wdr = "C-WDR", plain.txt = "PLAIN"
HOST_SCENARIO(update_scan)
{
    namespace fs = std::filesystem;
    if (!host.RequireUal()) return;
    if (!host.Check(host.ual.GetOverloadPathW && host.ual.AddVirtualFileForOverloadW, "the overload API is exported")) return;

    wchar_t buf[MAX_PATH] = {};
    if (!host.Check(host.ual.GetOverloadPathW(buf, MAX_PATH), "GetOverloadPathW reports the update folder")) return;
    fs::path updatePath(buf);
    fs::path game = fs::path(ExeDir());
    host.Check(_wcsicmp(updatePath.c_str(), (game / L"update").c_str()) == 0, "the update folder is <game>/update", ualtest::utf8(updatePath.wstring()));

    std::error_code ec;
    host.Check(fs::exists(updatePath, ec), "std::filesystem::exists(update)");
    host.Check(fs::is_directory(updatePath, ec), "std::filesystem::is_directory(update)");

    constexpr auto perms = fs::directory_options::skip_permission_denied | fs::directory_options::follow_directory_symlink;
    auto lower = [](std::wstring s) {
        for (auto& c : s) c = (wchar_t)towlower(c);
        return s;
    };
    auto slurp = [](const fs::path& p) {
        std::ifstream f(p, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(f), {});
    };

    // whole tree, as FusionFix iterates it
    std::vector<std::string> tree;
    for (const auto& it : fs::recursive_directory_iterator(updatePath, perms, ec))
    {
        if (ec) continue;
        auto rel = lower(fs::path(it.path()).lexically_relative(updatePath).generic_wstring());
        tree.push_back(ualtest::utf8(rel) + (it.is_directory(ec) ? "/" : ""));
    }
    host.Check(!ec, "recursive_directory_iterator completes", ec.message());
    std::sort(tree.begin(), tree.end());
    std::string listing;
    for (auto& t : tree) listing += t + ";";
    host.CheckEq(listing, std::string("common/;common/new.img/;common/new.img/c.wdr;pc/;pc/data/;pc/data/gtxd.img/;pc/data/gtxd.img/a.wtd;"
                                      "pc/data/gtxd.img/sub/;pc/data/gtxd.img/sub/b.wtd;plain.txt;update.txt;"),
                 "the update tree");

    // .img folders become archives, here the concatenated file contents
    int imgFolders = 0;
    for (const auto& it : fs::recursive_directory_iterator(updatePath, perms, ec))
    {
        if (ec) continue;
        fs::path folder = it.path();
        if (!fs::is_directory(it, ec) || _wcsicmp(folder.extension().c_str(), L".img") != 0) continue;
        ++imgFolders;
        std::vector<std::pair<std::string, std::string>> files;
        for (const auto& f : fs::recursive_directory_iterator(folder, perms, ec))
        {
            if (ec || !f.is_regular_file(ec)) continue;
            host.CheckEq((uint64_t)f.file_size(ec), (uint64_t)slurp(f.path()).size(), "file_size of " + f.path().filename().string());
            files.emplace_back(f.path().filename().string(), slurp(f.path()));
        }
        std::sort(files.begin(), files.end());
        std::string archive;
        for (auto& [n, d] : files) archive += n + "=" + d + ";";

        auto relative = folder.lexically_relative(updatePath);
        auto original = game / relative;
        original.replace_extension(".img");
        if (fs::exists(original, ec) && fs::is_regular_file(original, ec))
        {
            std::string merged = slurp(original) + "+" + archive;
            host.Check(host.ual.AddVirtualFileForOverloadW(relative.c_str(), (const uint8_t*)merged.data(), merged.size(), 1000), "merged archive added");
        }
        else
        {
            auto inGame = folder.lexically_relative(game);
            host.Check(host.ual.AddVirtualFileForOverloadW(inGame.c_str(), (const uint8_t*)archive.data(), archive.size(), 1000), "new archive added");
        }
    }
    host.CheckEq(imgFolders, 2, ".img folders found");

    // game reads through its own paths
    auto readA = [](const char* path) {
        std::string s;
        if (FILE* f = fopen(path, "rb"))
        {
            char b[256];
            size_t n;
            while ((n = fread(b, 1, sizeof(b), f)) > 0) s.append(b, n);
            fclose(f);
        }
        else
            s = "<cannot open>";
        return s;
    };
    host.CheckEq(readA("pc/data/gtxd.img"), std::string("ORIGINAL-IMG+a.wtd=A-WTD;b.wtd=B-WTD;"), "pc/data/gtxd.img is the merged archive");
    host.CheckEq(readA("update\\common\\new.img"),std::string("c.wdr=C-WDR;"), "update/common/new.img is the new archive");
    host.CheckEq(readA("update/plain.txt"), std::string("PLAIN"), "update/plain.txt");
    host.CheckEq(readA("plain.txt"), std::string("PLAIN"), "plain.txt is overloaded");
}
