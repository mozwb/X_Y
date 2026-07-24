#pragma once
#include<chrono>
#include<functional>
#include<thread>
#include<atomic>
#include<mutex>
#include<condition_variable>
namespace X_Y {
	class StopWatch {
    public:
		StopWatch();
        ~StopWatch();

		void Reset();
		
		void Pause();
		void Resume();

		float Seconds();
		float Milliseconds();
		float Microseconds();
		long long Nanoseconds();


		static float Measure(std::function<void()> func);
	private:
		using Clock = std::chrono::steady_clock;
		using TimePoint = Clock::time_point;
		using DurationNs = std::chrono::nanoseconds;

		// 计时起点
		TimePoint m_StartPoint;
		// 暂停前累计的纳秒时长
		DurationNs m_AccumulateNs{ 0 };
		// 是否处于暂停状态
		bool m_IsPaused = false;
	};
    // 原始时间数据，存储拆分后的时间数值
    struct TimeData
    {
        int year;    // 完整4位年份 2026
        int month;   // 1~12
        int day;     // 1~31
        int hour;    // 0~23
        int minute;  // 0~59
        int second;  // 0~59
    };
    class SysClock
    {
    public:
        // 获取当前本地系统时间
        static TimeData Now();

        /**
         * @brief 自定义格式化时间字符串
         * 占位符规则：
         * YY = 4位完整年份
         * MM = 两位月份(01~12)
         * DD = 两位日期(01~31)
         * hh = 两位小时(00~23)
         * mm = 两位分钟(00~59)
         * ss = 两位秒数(00~59)
         * 示例模板："YY-MM-DD hh:mm:ss" → "2026-06-18 14:25:09"
         */
        static std::string Format(const TimeData& data, const std::string& pattern);
        // 直接格式化当前时间
        static std::string NowFormat(const std::string& pattern);

        // 默认格式 YY-MM-DD hh:mm:ss
        static std::string ToString(const TimeData& data);
        static std::string NowString();
    private:
        // 数字转两位字符串，自动前补0
        static std::string ToTwoDigit(int num);
    };
    // ════════════════════════════════════════════════════════════
    // Ticker — 周期定时器，独立线程回调
    // 用于需要定期轮询或执行任务的场景
    // ════════════════════════════════════════════════════════════
    class Ticker {
    public:
        Ticker() = default;
        ~Ticker();

        // 禁止拷贝
        Ticker(const Ticker&) = delete;
        Ticker& operator=(const Ticker&) = delete;

        // ── 启动与结束 ──
        void Start(int intervalMs, std::function<void()> callback);
        void Stop();           // 请求停止，不等
        void Join();           // 等线程完全结束

        // ── 临时控制 ──
        void Pause();          // 暂停 callback，线程还在
        void Resume();         // 恢复 callback
        void Restart();                                          // 重启（用已有参数）
        void Restart(int intervalMs, std::function<void()> callback); // 重启并换参数

        // ── 放生 ──
        void Detach();

        // ── 状态 ──
        bool IsRunning() const { return m_Running.load(); }
        bool IsPaused() const { return m_Paused.load(); }
        bool IsJoinable() const { return m_Thread.joinable(); }

    private:
        void ThreadLoop(int intervalMs, std::function<void()> callback);

        std::thread m_Thread;
        std::atomic<bool> m_Running{ false };
        std::atomic<bool> m_Paused{ false };
        std::mutex m_Mutex;
        std::condition_variable m_CV;
        int m_IntervalMs = 0;
        std::function<void()> m_Callback;
    };

    class Timestep
    {
    public:
        Timestep(float time = 0.0f)
            : m_Time(time)
        {
        }

        operator float() const { return m_Time; }

        float GetSeconds() const { return m_Time; }
        float GetMilliseconds() const { return m_Time * 1000.0f; }
    private:
        float m_Time;
    };
}