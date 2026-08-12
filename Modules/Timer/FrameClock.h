#pragma once
#include <chrono>

namespace X_Y
{
    class FrameClock
    {
    public:
        FrameClock();

        // 记录一帧结束：返回本帧耗时(秒)。
        // dt 会被限制在 [0, 最大dt上限] 内，防卡顿跳帧导致物理爆炸。
        float Tick();

        // ── 配置（需在 Start 前或运行时设置，立即生效）──
        // 设置最大 dt(秒)。超过则钳制。0 或负 = 不限。
        void SetMaxDelta(float maxDelta);
        float GetMaxDelta() const;

        // 设置目标帧率(帧/秒)。Tick() 内部会 sleep 到该节奏。
        // 0 或负 = 不限制帧率。
        void SetTargetFPS(float fps);
        float GetTargetFPS() const;

    private:
        using Clock = std::chrono::steady_clock;
        using Sec = std::chrono::duration<float>;

        Clock::time_point m_LastTick;
        float m_MaxDelta = 0.05f; // 默认最大 dt 0.05s(20fps下限)
        float m_TargetFps = 0.0f; // 默认不限制
        bool m_First = true;      // 第一次 Tick 只初始化，不产生 dt
    };
}