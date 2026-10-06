// Event log written by the probe and host to UAL_TEST_REPORT, parsed by the runner after exit.
// One record per line as tab-separated key=value pairs. Values are byte strings with '\\', TAB,
// CR, LF and other control bytes escaped, so any file content fits.
#pragma once

#include <windows.h>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace ualtest
{
    constexpr wchar_t kReportEnv[] = L"UAL_TEST_REPORT";

    struct Record
    {
        std::map<std::string, std::string> fields;

        const std::string& get(const std::string& k) const
        {
            static const std::string empty;
            auto it = fields.find(k);
            return it == fields.end() ? empty : it->second;
        }
        bool has(const std::string& k) const { return fields.count(k) != 0; }
        long long num(const std::string& k, long long def = -1) const
        {
            auto& v = get(k);
            if (v.empty()) return def;
            return _strtoi64(v.c_str(), nullptr, 0);
        }
        std::string& operator[](const std::string& k) { return fields[k]; }
    };

    inline std::string utf8(std::wstring_view s)
    {
        if (s.empty()) return {};
        int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
        std::string r(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), r.data(), n, nullptr, nullptr);
        return r;
    }

    inline std::wstring wide(std::string_view s)
    {
        if (s.empty()) return {};
        int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        std::wstring r(n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), r.data(), n);
        return r;
    }

    inline std::string escape(std::string_view s)
    {
        static const char* hex = "0123456789ABCDEF";
        std::string r;
        r.reserve(s.size());
        for (unsigned char c : s)
        {
            switch (c)
            {
            case '\\': r += "\\\\"; break;
            case '\t': r += "\\t"; break;
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            default:
                if (c < 0x20 || c == 0x7F)
                {
                    r += "\\x";
                    r += hex[c >> 4];
                    r += hex[c & 15];
                }
                else
                    r += (char)c;
            }
        }
        return r;
    }

    inline std::string unescape(std::string_view s)
    {
        std::string r;
        r.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (s[i] != '\\' || i + 1 >= s.size()) { r += s[i]; continue; }
            char n = s[++i];
            switch (n)
            {
            case 't': r += '\t'; break;
            case 'n': r += '\n'; break;
            case 'r': r += '\r'; break;
            case 'x':
                if (i + 2 < s.size())
                {
                    auto hv = [](char h) { return h >= 'A' ? (h & ~0x20) - 'A' + 10 : h - '0'; };
                    r += (char)((hv(s[i + 1]) << 4) | hv(s[i + 2]));
                    i += 2;
                }
                break;
            default: r += n; break;
            }
        }
        return r;
    }

    inline std::string serialize(const Record& rec)
    {
        std::string line;
        for (auto& [k, v] : rec.fields)
        {
            if (!line.empty()) line += '\t';
            line += k;
            line += '=';
            line += escape(v);
        }
        return line;
    }

    inline Record parse_line(std::string_view line)
    {
        Record rec;
        size_t start = 0;
        while (start <= line.size())
        {
            auto end = line.find('\t', start);
            if (end == std::string_view::npos) end = line.size();
            auto item = line.substr(start, end - start);
            auto eq = item.find('=');
            if (eq != std::string_view::npos)
                rec.fields[std::string(item.substr(0, eq))] = unescape(item.substr(eq + 1));
            start = end + 1;
        }
        return rec;
    }

    // FILE_APPEND_DATA keeps concurrent writers from interleaving within a line
    inline bool append_record(const std::wstring& path, const Record& rec)
    {
        if (path.empty()) return false;
        std::string line = serialize(rec) + "\n";
        for (int attempt = 0; attempt < 50; ++attempt)
        {
            HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h != INVALID_HANDLE_VALUE)
            {
                DWORD written = 0;
                BOOL ok = WriteFile(h, line.data(), (DWORD)line.size(), &written, nullptr);
                CloseHandle(h);
                return ok && written == line.size();
            }
            if (GetLastError() != ERROR_SHARING_VIOLATION) return false;
            Sleep(2);
        }
        return false;
    }

    inline std::wstring report_path_from_env()
    {
        wchar_t buf[2048];
        DWORD n = GetEnvironmentVariableW(kReportEnv, buf, (DWORD)std::size(buf));
        if (n == 0 || n >= std::size(buf)) return {};
        return std::wstring(buf, n);
    }

    inline std::vector<Record> parse_records(std::string_view text)
    {
        std::vector<Record> out;
        size_t start = 0;
        while (start < text.size())
        {
            auto end = text.find('\n', start);
            if (end == std::string_view::npos) end = text.size();
            auto line = text.substr(start, end - start);
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            if (!line.empty()) out.push_back(parse_line(line));
            start = end + 1;
        }
        return out;
    }
}
