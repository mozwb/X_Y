#include "FrameClock.h"
#include <thread>

namespace X_Y
{

    FrameClock::FrameClock()
    {
        m_LastTick = Clock::now();
    }

    float FrameClock::Tick()
    {
        auto now = Clock::now();
        if (m_First)
        {
            m_First = false;
            m_LastTick = now;
            return 0.0f; // 首帧不产生 dt
        }

        // 1) 算本帧耗时
        float dt = std::chrono::duration_cast<Sec>(now - m_LastTick).count();
        m_LastTick = now;

        // 2) 上限钳制
        if (m_MaxDelta > 0.0f && dt > m_MaxDelta)
            dt = m_MaxDelta;

        // 3) 目标帧率：等到该帧应输出的时刻再返回
        if (m_TargetFps > 0.0f)
        {
            float frameMs = 1000.0f / m_TargetFps; // 每帧应占毫秒
            float usedMs = dt * 1000.0f;           // 本帧已用毫秒
            if (usedMs < frameMs)
                std::this_thread::sleep_for(std::chrono::milliseconds((long long)(frameMs - usedMs)));
        }

        return dt;
    }

    void FrameClock::SetMaxDelta(float seconds) { m_MaxDelta = seconds; }
    float FrameClock::GetMaxDelta() const { return m_MaxDelta; }
    void FrameClock::SetTargetFPS(float fps) { m_TargetFps = fps; }
    float FrameClock::GetTargetFPS() const { return m_TargetFps; }

}