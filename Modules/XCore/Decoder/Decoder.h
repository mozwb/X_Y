#pragma once
#include "../Memory/Buffer.h"

namespace X_Y
{
    class Decoder
    {
    public:
        template <typename Handle>
        static void Decode(const Buffer &source, Buffer &target, Handle handle)
        {
            handle(source, target);
        }
    };
}