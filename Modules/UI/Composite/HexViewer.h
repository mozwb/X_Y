#pragma once
#include "UI/Container/Container.h"
#include <string>

namespace X_Y
{

    class HexViewer : public Container
    {
    public:
        HexViewer();
        ~HexViewer() override;

    private:
        std::string file;
    };
}