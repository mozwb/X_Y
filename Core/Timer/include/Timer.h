#pragma once
#include<chrono>
#include<functional>
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
}