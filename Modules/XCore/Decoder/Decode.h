#pragma once
#include "../Memory/Buffer.h"
#include "../Stream/Stream.h"
#include <array>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
namespace X_Y
{
    template <typename Source>
    class DecoderBlueprint
    {
    public:
        virtual ~DecoderBlueprint() = default;
        virtual void Decode(Source &source) = 0;
    };

    // 因为可能会有一堆解析格式，会有一堆调用方式，这里提供一个通用的接口，
    // 其实还是要实现解析方式和返回的数据格式，但是从调用上看都收敛于此，会比较统一吧。
    // 之后会写好一些常用的解析方式和数据结构组合，方便直接使用
    class Decoder
    {
    public:
        template <typename Blueprint, typename Source>
            requires std::is_default_constructible_v<Blueprint>
        static auto Decode(Source &&source)
        {
            Blueprint blueprint{};
            Decode(std::forward<Source>(source), blueprint);
            return blueprint;
        }

        template <typename Blueprint, typename Source, typename Target>
            requires std::is_default_constructible_v<Blueprint>
        static decltype(auto) Decode(Source &&source, Target &target)
        {
            return Decode(
                std::forward<Source>(source),
                target,
                Blueprint{});
        }

        template <typename Source, typename Blueprint>
        static decltype(auto) Decode(Source &&source, Blueprint &&blueprint)
        {
            if constexpr (requires {
                              std::forward<Blueprint>(blueprint).Decode(
                                  std::forward<Source>(source));
                          })
            {
                return std::forward<Blueprint>(blueprint).Decode(
                    std::forward<Source>(source));
            }
            else
            {
                return std::invoke(
                    std::forward<Blueprint>(blueprint),
                    std::forward<Source>(source));
            }
        }

        template <typename Source, typename Target, typename Blueprint>
        static decltype(auto) Decode(Source &&source, Target &target, Blueprint &&blueprint)
        {
            if constexpr (requires {
                              std::forward<Blueprint>(blueprint).Decode(
                                  std::forward<Source>(source), target);
                          })
            {
                return std::forward<Blueprint>(blueprint).Decode(
                    std::forward<Source>(source), target);
            }
            else if constexpr (std::is_invocable_v<Blueprint, Source, Target &>)
            {
                return std::invoke(
                    std::forward<Blueprint>(blueprint),
                    std::forward<Source>(source),
                    target);
            }
            else
            {
                target = Decode(
                    std::forward<Source>(source),
                    std::forward<Blueprint>(blueprint));
                return (target);
            }
        }

        template <typename Blueprint, typename Target>
        static decltype(auto) DecodeNext(Blueprint &blueprint, Target &target)
        {
            if constexpr (requires {
                              blueprint.DecodeNext(target);
                          })
            {
                return blueprint.DecodeNext(target);
            }
            else
            {
                return blueprint.ReadNext(target);
            }
        }
    };
}