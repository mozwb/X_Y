#pragma once
#include <cstdint>

namespace X_Y {

    // 窗口显示方式（跨平台抽象）
    
    enum class ShowCmd {
        Hide,
        Normal,
        Minimized,
        Maximized,
        Show,
        Default
    };

    // 光标样式
    
    enum class CursorStyle {
        Arrow,
        SizeWE,
        SizeNS,
        SizeAll,
        Hand,
        IBeam
    };

    // 窗口风格（位标志，平台内部转换）
    
    enum class WindowStyleFlag : uint32_t {
        None         = 0,
        Overlapped   = 1 << 0,
        Child        = 1 << 1,
        Popup        = 1 << 2,
        Borderless   = 1 << 3,
        Resizable    = 1 << 4,
        ClipChildren = 1 << 5,
        ClipSiblings = 1 << 6,
        Visible      = 1 << 7,
    };

    inline WindowStyleFlag operator|(WindowStyleFlag a, WindowStyleFlag b) {
        return static_cast<WindowStyleFlag>(
            static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
        );
    }

    inline bool HasFlag(WindowStyleFlag value, WindowStyleFlag flag) {
        return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
    }

    // 平台窗口实现——纯虚接口
    // 不依赖 BaseWin，只操作原生窗口句柄
    
    class WindowImpl {
    public:
        virtual ~WindowImpl() = default;

        // ── 窗口生命周期 ──────────────────────────
        
        virtual bool Create(const char* title, uint32_t width, uint32_t height,
            WindowStyleFlag style, void* parentHandle,
            void* createParam) = 0;
        virtual bool Show(ShowCmd cmd) = 0;
        virtual void Close() = 0;
        virtual void Destroy() = 0;
        virtual void SetTitle(const char* title) = 0;

        // ── 原生句柄 ──────────────────────────────
        
        virtual void* GetNativeHandle() const = 0;
        virtual void* GetParentNativeHandle() const = 0;

        // ── 窗口父子关系 ──────────────────────────
        
        virtual bool SetParent(void* newParent) = 0;

        // ── 窗口尺寸 ──────────────────────────────
        
        virtual void GetClientRect(int& left, int& top, int& right, int& bottom) const = 0;
        virtual uint32_t GetClientWidth() const = 0;
        virtual uint32_t GetClientHeight() const = 0;
        virtual void SetClientSize(uint32_t width, uint32_t height) = 0;

        // ── 坐标转换 ──────────────────────────────
        
        virtual void ScreenToClient(int& x, int& y) const = 0;
        virtual void ClientToScreen(int& x, int& y) const = 0;

        // ── 重绘请求 ──────────────────────────────
        virtual void RequestRepaint() = 0;

        // ── 鼠标 & 光标 ───────────────────────────
        
        virtual void CaptureMouse() = 0;
        virtual void ReleaseMouseCapture() = 0;
        virtual void SetCursorStyle(CursorStyle style) = 0;

        // ── 静态工具（全局操作，无需实例） ────────
        static void GetMouseScreenPos(int& x, int& y);
        static void ReleaseGlobalMouseCapture();
        virtual void MoveAndResize(int x, int y, int w, int h, bool noZOrder) = 0;
    };

    // 平台工厂
    
    class PlatformFactory {
    public:
        static WindowImpl* CreateWindowImpl();
    };

}
