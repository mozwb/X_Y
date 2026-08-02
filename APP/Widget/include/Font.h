#pragma once
#include <string>
#include <memory>

namespace X_Y {

    // ── 字体描述 ──
    // 描述一款字体的外观参数。传 desc 给 Font 创建具体字体。
    struct FontDesc {
        // 质量档位（对应 Win32 LOGFONT.lfQuality 语义）
        enum Quality {
            Default,          // 系统默认
            NonAntiAliased,   // 关闭抗锯齿（锯齿明显）
            AntiAliased,      // 标准抗锯齿（灰度）
            ClearType,        // ClearType（LCD 屏最清晰，默认推荐）
        };

        std::string family = "Microsoft YaHei UI";  // 字体族名
        int  size    = 14;                          // 字号（像素坐标下的字高）
        bool bold    = false;                       // 粗体
        Quality quality = ClearType;                // 默认抗锯齿

        // 可选：自定义字体文件路径（.ttf/.otf）。
        // 非空时用 AddFontResourceEx(FR_PRIVATE) 私有加载，不污染系统字体表。
        std::string filePath;
    };

    // ── 字体抽象接口（纯虚，不依赖平台类型） ──
    class FontImpl {
    public:
        virtual ~FontImpl() = default;

        virtual const FontDesc& GetDesc() const = 0;
        virtual void*  GetNativeHandle() const = 0;   // Win32 = HFONT
        // 字体度量：供布局用（行高 / 平均字符宽），单位像素
        virtual int GetHeight() const = 0;      // 行高（推荐行距）
        virtual int GetAscent() const = 0;
        virtual int GetAvgCharWidth() const = 0;
    };

    // ── 字体工厂 ──
    class FontFactory {
    public:
        static FontImpl* Create(const FontDesc& desc);
    };

    // ── Font 轻量包装（隐藏平台实现，RAII 持有） ──
    class Font {
    public:
        Font() : m_Impl(FontFactory::Create(FontDesc{})) {}
        explicit Font(const FontDesc& desc)
            : m_Impl(FontFactory::Create(desc)) {}

        const FontDesc& GetDesc() const { return m_Impl->GetDesc(); }
        void*  GetNativeHandle() const { return m_Impl->GetNativeHandle(); }
        int GetHeight() const { return m_Impl->GetHeight(); }
        int GetAscent() const { return m_Impl->GetAscent(); }
        int GetAvgCharWidth() const { return m_Impl->GetAvgCharWidth(); }

    private:
        std::unique_ptr<FontImpl> m_Impl;
    };

} // namespace X_Y
