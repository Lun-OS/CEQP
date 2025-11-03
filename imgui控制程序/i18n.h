#pragma once
#include <string>
#include <unordered_map>

// 简易本地化支持（读取 Language/zh-cn.po）
struct I18N {
    enum class Lang { EN, ZH };
    static Lang current;
    static bool zhLoaded;
    static std::unordered_map<std::string, std::string> zhMap;

    static void setLang(Lang l);
    static std::wstring getExeDirW();
    static bool readFileUtf8(const std::wstring& wpath, std::string& out);
    static std::string unquote(const std::string& s);
    static bool parsePo(const std::string& content, std::unordered_map<std::string, std::string>& map);
    static bool loadZh();
    static std::string tr(const std::string& s);
};