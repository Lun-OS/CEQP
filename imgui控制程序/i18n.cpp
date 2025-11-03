#include "i18n.h"
#include <windows.h>
#include <sstream>
#include <algorithm>
#include <cctype>

// 静态成员变量定义
I18N::Lang I18N::current = I18N::Lang::EN;
bool I18N::zhLoaded = false;
std::unordered_map<std::string, std::string> I18N::zhMap;

void I18N::setLang(Lang l) {
    current = l;
    if (current == Lang::ZH && !zhLoaded) loadZh();
}

std::wstring I18N::getExeDirW() {
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    std::wstring path(buf);
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) path.resize(pos);
    return path;
}

bool I18N::readFileUtf8(const std::wstring& wpath, std::string& out) {
    HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE || size == 0) { CloseHandle(h); return false; }
    out.resize(size);
    DWORD read = 0;
    BOOL ok = ReadFile(h, &out[0], size, &read, NULL);
    CloseHandle(h);
    return ok && read == size;
}

std::string I18N::unquote(const std::string& s) {
    // 移除两端的引号，并简单处理转义字符（只处理 \" 和 \\）
    std::string r = s;
    if (!r.empty() && r.front() == '"') r.erase(r.begin());
    if (!r.empty() && r.back() == '"') r.pop_back();
    std::string o; o.reserve(r.size());
    for (size_t i = 0; i < r.size(); ++i) {
        if (r[i] == '\\' && i + 1 < r.size()) {
            char c = r[i + 1];
            if (c == '"' || c == '\\') { o.push_back(c); ++i; continue; }
        }
        o.push_back(r[i]);
    }
    return o;
}

bool I18N::parsePo(const std::string& content, std::unordered_map<std::string, std::string>& map) {
    std::istringstream iss(content);
    std::string line;
    std::string curId;
    while (std::getline(iss, line)) {
        // 去掉前后空白
        auto ltrim = [](std::string& x){ x.erase(x.begin(), std::find_if(x.begin(), x.end(), [](int ch){ return !std::isspace(ch); })); };
        auto rtrim = [](std::string& x){ x.erase(std::find_if(x.rbegin(), x.rend(), [](int ch){ return !std::isspace(ch); }).base(), x.end()); };
        ltrim(line); rtrim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("msgid", 0) == 0) {
            size_t p = line.find(' ');
            if (p != std::string::npos) {
                std::string q = line.substr(p + 1);
                curId = unquote(q);
            }
        } else if (line.rfind("msgstr", 0) == 0) {
            if (curId.empty()) continue;
            size_t p = line.find(' ');
            if (p != std::string::npos) {
                std::string q = line.substr(p + 1);
                std::string val = unquote(q);
                if (!val.empty()) map[curId] = val;
                curId.clear();
            }
        }
    }
    return !map.empty();
}

bool I18N::loadZh() {
    std::wstring po = getExeDirW() + L"\\Language\\zh-cn.po";
    std::string content;
    if (!readFileUtf8(po, content)) { zhLoaded = false; return false; }
    zhMap.clear();
    zhLoaded = parsePo(content, zhMap);
    return zhLoaded;
}

std::string I18N::tr(const std::string& s) {
    if (current == Lang::ZH) {
        auto it = zhMap.find(s);
        if (it != zhMap.end()) return it->second;
    }
    return s;
}