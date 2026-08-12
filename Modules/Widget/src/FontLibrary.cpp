#include "Widget/FontLibrary.h"

namespace X_Y {

    FontLibrary& FontLibrary::Instance() {
        static FontLibrary s_Instance;
        return s_Instance;
    }

    const Font& FontLibrary::GetDefault() {
        if (!m_Default)
            m_Default.reset(new Font(FontDesc{}));   // 默认描述 = 微软雅黑 ClearType
        return *m_Default;
    }

    const Font& FontLibrary::Get(const std::string& filePath,
                                 int size, bool bold) {
        FontDesc desc;
        desc.filePath = filePath;
        desc.size     = size;
        desc.bold     = bold;

        std::string key = filePath.empty() ? MakeKey(desc) : filePath;
        auto it = m_ByPath.find(key);
        if (it != m_ByPath.end())
            return *it->second;

        auto f = std::make_unique<Font>(desc);
        auto& ref = *f;
        m_ByPath.emplace(key, std::move(f));
        return ref;
    }

    void FontLibrary::Register(const FontDesc& desc, const std::string& name) {
        if (name.empty())
            return;
        m_ByName[name] = std::make_unique<Font>(desc);
    }

    const Font* FontLibrary::GetByName(const std::string& name) const {
        auto it = m_ByName.find(name);
        return it != m_ByName.end() ? it->second.get() : nullptr;
    }

    std::string FontLibrary::MakeKey(const FontDesc& desc) const {
        // 无路径（系统字体）时，用 族名|尺寸|粗体|质量 作缓存键
        return desc.family + "|" + std::to_string(desc.size) + "|" +
               (desc.bold ? "b" : "n") + "|" + std::to_string(desc.quality);
    }

} // namespace X_Y
