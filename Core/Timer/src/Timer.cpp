#pragma once
#include<Timer/include/Timer.h>
#include<Log/include/XYLog.h>
namespace X_Y {
	StopWatch::StopWatch()
	{
		Reset();
	}
    StopWatch::~StopWatch() {
        // @@ 析构留空 —— 日志由 ProfileScope 等 RAII 包装类统一处理
    }
    void StopWatch::Reset()
	{
		m_StartPoint = Clock::now();
		m_AccumulateNs = DurationNs(0);
		m_IsPaused = false;
	}

	void StopWatch::Pause()
	{
		if (m_IsPaused)
			return;

		// 累加本次运行时长
		auto now = Clock::now();
		m_AccumulateNs += std::chrono::duration_cast<DurationNs>(now - m_StartPoint);
		m_IsPaused = true;
	}

	void StopWatch::Resume()
	{
		if (!m_IsPaused)
			return;

		m_StartPoint = Clock::now();
		m_IsPaused = false;
	}

	long long StopWatch::Nanoseconds()
	{
		DurationNs total = m_AccumulateNs;
		if (!m_IsPaused)
		{
			auto now = Clock::now();
			total += std::chrono::duration_cast<DurationNs>(now - m_StartPoint);
		}
		return total.count();
	}

	float StopWatch::Microseconds()
	{
		return static_cast<float>(Nanoseconds()) / 1000.0f;
	}

	float StopWatch::Milliseconds()
	{
		return static_cast<float>(Nanoseconds()) / 1000000.0f;
	}

	float StopWatch::Seconds()
	{
		return static_cast<float>(Nanoseconds()) / 1000000000.0f;
	}

	float StopWatch::Measure(std::function<void()> func)
	{
		StopWatch watch;
		func();
		return watch.Milliseconds();
	}




    TimeData SysClock::Now()
    {
        auto tp = std::chrono::system_clock::now();
        std::time_t stamp = std::chrono::system_clock::to_time_t(tp);
        std::tm local = *std::localtime(&stamp);

        TimeData data{};
        data.year = local.tm_year + 1900;
        data.month = local.tm_mon + 1;
        data.day = local.tm_mday;
        data.hour = local.tm_hour;
        data.minute = local.tm_min;
        data.second = local.tm_sec;
        return data;
    }

    std::string SysClock::ToTwoDigit(int num)
    {
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << num;
        return oss.str();
    }

    std::string SysClock::Format(const TimeData& data, const std::string& pattern)
    {
        std::string output = pattern;
        std::string y4 = std::to_string(data.year);
        std::string m2 = ToTwoDigit(data.month);
        std::string d2 = ToTwoDigit(data.day);
        std::string h2 = ToTwoDigit(data.hour);
        std::string min2 = ToTwoDigit(data.minute);
        std::string s2 = ToTwoDigit(data.second);

        // 依次替换占位符
        size_t pos;
        // YY 完整4位年
        while ((pos = output.find("YY")) != std::string::npos)
            output.replace(pos, 2, y4);
        // MM 月份
        while ((pos = output.find("MM")) != std::string::npos)
            output.replace(pos, 2, m2);
        // DD 日期
        while ((pos = output.find("DD")) != std::string::npos)
            output.replace(pos, 2, d2);
        // hh 小时
        while ((pos = output.find("hh")) != std::string::npos)
            output.replace(pos, 2, h2);
        // mm 分钟
        while ((pos = output.find("mm")) != std::string::npos)
            output.replace(pos, 2, min2);
        // ss 秒
        while ((pos = output.find("ss")) != std::string::npos)
            output.replace(pos, 2, s2);

        return output;
    }

    std::string SysClock::NowFormat(const std::string& pattern)
    {
        return Format(Now(), pattern);
    }

    std::string SysClock::ToString(const TimeData& data)
    {
        return Format(data, "YY-MM-DD hh:mm:ss");
    }

    std::string SysClock::NowString()
    {
        return ToString(Now());
    }
}