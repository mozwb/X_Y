#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "Font.h"

namespace X_Y {

    // ── 字体库（全局单例）──
    // 统一管理字体，UI 组件不各自持有 Font，避免重复加载。
    // 取字方式：
    //   1. GetDefault() —— 内置默认字体（微软雅黑，系统字体，无文件路径）。
    //   2. Get(filePath, size, bold) —— 按字体文件路径取；未加载则自动加载并缓存。
    //      filePath 为空 → 回落为"族名描述"取系统字体。
    // 后端选择在工厂按 desc.filePath 是否为空决定：
    //   有路径 → 理想走 FreeType(未接入时暂退 GDI)；无路径 → GDI(系统字体)。

    class FontLibrary {
    public:
        static FontLibrary& Instance();

        // 默认字体（微软雅黑 ClearType，无文件路径 = 系统字体 GDI）
        const Font& GetDefault();

        // 按路径取字体；未加载则创建并缓存。filePath 为空时按族名/size/bold 构建。
        const Font& Get(const std::string& filePath,
                        int size = 14, bool bold = false);

        // 显式注册：按 name 注册一款已有 desc 的字体，之后 GetByName(name) 取。
        // 用于预设几种业务字体（如 "Title"/"Body"）。可选，不强制使用。
        void Register(const FontDesc& desc, const std::string& name);
        const Font* GetByName(const std::string& name) const;

    private:
        FontLibrary() = default;
        ~FontLibrary() = default;
        FontLibrary(const FontLibrary&) = delete;
        FontLibrary& operator=(const FontLibrary&) = delete;

        std::string MakeKey(const FontDesc& desc) const; // 缓存键

        std::unique_ptr<Font> m_Default;
        std::unordered_map<std::string, std::unique_ptr<Font>> m_ByPath; // 键=路径
        std::unordered_map<std::string, std::unique_ptr<Font>> m_ByName; // 键=业务名
    };

} // namespace X_Y
