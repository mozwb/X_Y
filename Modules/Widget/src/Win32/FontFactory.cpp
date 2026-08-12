#include "Widget/Font.h"
#include "Win32/FontWin32.h"
#include "Win32/FontFreeType.h"

#ifdef DrawText
#undef DrawText
#endif

namespace X_Y {

    // 字体工厂：按描述分发后端。
    //   有 filePath（.ttf/.otf）→ FreeType（per-pixel alpha 真透明文字）
    //   无 filePath（系统字体）→ GDI（alpha 截断）
    FontImpl* FontFactory::Create(const FontDesc& desc) {
        if (!desc.filePath.empty())
            return new FontFreeType(desc);
        return new FontWin32(desc);
    }

} // namespace X_Y
