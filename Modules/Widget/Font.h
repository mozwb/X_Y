#pragma once
#include <string>
#include <memory>
#include "CanvasImpl.h"   // CanvasTarget（文字绘制目标）

#ifdef DrawText
#undef DrawText
#endif

namespace X_Y {

    // ── 字体描述 ──
    // 描述一款字体的外观参数。传 desc 给 Font 创建具体字体。

    struct FontDesc {
        // 质量档位（对应 Win32 LOGFONT.lfQuality 语义；FreeType 后端忽略）

        enum Quality {
            Default,          // 系统默认
            NonAntiAliased,   // 关闭抗锯齿（锯齿明显）
            AntiAliased,      // 标准抗锯齿（灰度）
            ClearType,        // ClearType（LCD 屏最清晰，默认推荐）
        };

        std::string family = "Microsoft YaHei UI";  // 字体族名（系统字体用）
        int  size    = 14;                          // 字号（像素坐标下的字高）
        bool bold    = false;                       // 粗体
        Quality quality = ClearType;                // 默认抗锯齿

        // 可选：自定义字体文件路径（.ttf/.otf）。
        // 非空时优先按文件加载（FreeType / AddFontResourceEx 私有加载）。

        std::string filePath;
    };

    // ── 字体抽象接口（纯虚，不依赖平台类型）──
    // 职责：描述字体 + **绘制文字**（DrawText/FillText/MeasureText）。
    // 后端：GDI（TextOut，alpha 截断）/ FreeType（per-pixel alpha，透明）。
    // 绘制目标统一走 CanvasTarget（软件 ARGB 缓冲 + 可选 GDI 桥 DC）。

    class FontImpl {
    public:
        virtual ~FontImpl() = default;

        virtual const FontDesc& GetDesc() const = 0;

        // 字体度量：供布局用（行高 / 平均字符宽），单位像素
        virtual int GetHeight() const = 0;
        virtual int GetAscent() const = 0;
        virtual int GetAvgCharWidth() const = 0;

        // ── 文字绘制 ──
        // 把 text 画到目标缓冲 target 的 (x,y) 处（逻辑坐标；后端内部按需 DPI 缩放）。
        // color = 0xAARRGGBB。GDI 后端 alpha 截断（不产生透明）；FreeType 用 alpha 合成。
        // const：文字绘制不改变字体状态（只读写 target 缓冲）。
        virtual void DrawText(const CanvasTarget& target, int x, int y,
                              const char* text, uint32_t color) const = 0;
        virtual void DrawText(const CanvasTarget& target, int x, int y,
                              const wchar_t* text, uint32_t color) const = 0;

        // 组合拳：铺背景 + 写字（背景一次传入，ClearType 用同一背景色）
        // (x,y,w,h)=背景矩形；(tx,ty)=文字起点，可与背景错开(缩进/偏移)。
        // 窄/宽字符两版，窄版在平台实现层转宽。
        virtual void FillText(const CanvasTarget& target,
            int x, int y, int w, int h, int tx, int ty,
            const char* text, uint32_t textColor, uint32_t bgColor) const = 0;
        virtual void FillText(const CanvasTarget& target,
            int x, int y, int w, int h, int tx, int ty,
            const wchar_t* text, uint32_t textColor, uint32_t bgColor) const = 0;

        // 测量文字宽度（用本字体），返回逻辑像素宽。供折行/布局判断。
        virtual int MeasureText(const char*  text) const = 0;
        virtual int MeasureText(const wchar_t* text) const = 0;
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
        int GetHeight() const { return m_Impl->GetHeight(); }
        int GetAscent() const { return m_Impl->GetAscent(); }
        int GetAvgCharWidth() const { return m_Impl->GetAvgCharWidth(); }

        // ── 文字绘制（薄转发到 FontImpl；目标由 Canvas 组装 CanvasTarget 传入）──
        void DrawText(const CanvasTarget& target, int x, int y,
                      const char* text, uint32_t color) const {
            m_Impl->DrawText(target, x, y, text, color);
        }
        void DrawText(const CanvasTarget& target, int x, int y,
                      const wchar_t* text, uint32_t color) const {
            m_Impl->DrawText(target, x, y, text, color);
        }
        void FillText(const CanvasTarget& target,
            int x, int y, int w, int h, int tx, int ty,
            const char* text, uint32_t textColor, uint32_t bgColor) const {
            m_Impl->FillText(target, x, y, w, h, tx, ty, text, textColor, bgColor);
        }
        void FillText(const CanvasTarget& target,
            int x, int y, int w, int h, int tx, int ty,
            const wchar_t* text, uint32_t textColor, uint32_t bgColor) const {
            m_Impl->FillText(target, x, y, w, h, tx, ty, text, textColor, bgColor);
        }
        int MeasureText(const char*  text) const { return m_Impl->MeasureText(text); }
        int MeasureText(const wchar_t* text) const { return m_Impl->MeasureText(text); }

    private:
        std::unique_ptr<FontImpl> m_Impl;
    };

} // namespace X_Y
