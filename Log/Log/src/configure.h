#pragma once
// 配置里用到的类型
#include<iostream>
#include<string>
#include <chrono>
#include <ctime>
#include<ostream>
#include<fstream>

namespace LogConfigur {
	using str = std::string;
	template<typename T> using vec = std::vector<T>;
	//继承该类的函数，使用<<会自动使用To_Str调用ToString函数（如果有的话），否则输出NO_TO_STRING
	//但是只能类内部使用，外部的<<是普通的输出
	struct CustomLogBase {};
	struct TIME:public  CustomLogBase{
		static str now() {
			auto now = std::chrono::system_clock::now();
			std::time_t t = std::chrono::system_clock::to_time_t(now);
			char buf[32];
			std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
			return buf;
		}

		str toString() const {
			return now();
		}
	};
	struct DATE :public  CustomLogBase {
		static str now() {
			auto now = std::chrono::system_clock::now();
			std::time_t t = std::chrono::system_clock::to_time_t(now);
			char buf[32];
			std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
			return buf;
		}

		// 支持 toString（给你之前的自动替换用）
		str toString() const {
			return now(); // 直接返回当前时间
		}
	};


	template<typename LEVEL_TYPE,typename SELF_TYPE>
	struct LEVEL :public CustomLogBase {
		LEVEL() = delete;
		static str model;
		static void setModel(str m) { model = m; }
		static str getModle() { return model; }
		static str toString() { return "LEVEL"; }
	};
	template<typename LEVEL_TYPE, typename SELF_TYPE> str LEVEL<LEVEL_TYPE,SELF_TYPE>::model;
#define LOG_LEVEL_ALL(SELF_TYPE) \
public: \
    struct Trace : LEVEL<Trace, SELF_TYPE> { \
        static str toString() { return "TRACE"; } \
    }; \
    struct Debug : LEVEL<Debug, SELF_TYPE> { \
        static str toString() { return "DEBUG"; } \
    }; \
    struct Info : LEVEL<Info, SELF_TYPE> { \
        static str toString() { return "INFO"; } \
    }; \
    struct Warn : LEVEL<Warn, SELF_TYPE> { \
        static str toString() { return "WARN"; } \
    }; \
    struct Error : LEVEL<Error, SELF_TYPE> { \
        static str toString() { return "ERROR"; } \
    }; \
    struct Fatal : LEVEL<Fatal, SELF_TYPE> { \
        static str toString() { return "FATAL"; } \
    };

	#define LOG_LEVEL_EXT(LevelName,SELF_TYPE) \
	struct LevelName : LogConfigur::LEVEL<LevelName, SELF_TYPE> { \
		static std::string toString() { return #LevelName; } \
	};
	
	struct DEVICE:public CustomLogBase{
		std::ostream* os=&std::cout;
		DEVICE() = default;
		DEVICE(std::ostream& os) :os(&os) {}
		//或许拓展要写
		virtual ~DEVICE() = default;
		virtual str toString() const {
			return	"DEVICE";
		};
		void Log(str message) {
			*os << message << std::endl;
		}
	};
	struct Console :public DEVICE {
		Console(std::ostream& os = std::cout) :DEVICE(os) {}
		str toString() const  override { return "Console"; } 
	};
	struct File :public DEVICE {
		// 成员变量
		std::string _filename;
		std::ofstream file;
		File(
			const std::string& filename,
			std::ios_base::openmode mode = std::ios::out
		)
		:          
			DEVICE(),
			_filename(filename),
			file(filename, mode)
		{
			if (!file.is_open()) {
				throw std::runtime_error("打开文件失败: " + filename);
			}
			os = &file;
		}
		str toString() const  override { return file.is_open()?_filename:"文件打开失败"; }
	};

	struct LogDevices :public CustomLogBase {
		std::vector<std::unique_ptr<DEVICE>> devices;

		// 1. 接收裸指针（别人new的）
		void add(DEVICE* dev) {
			if (dev) devices.emplace_back(dev);
		}

		// 2. 接收 unique_ptr（安全转移所有权）
		void add(std::unique_ptr<DEVICE> dev) {
			if (dev) devices.push_back(std::move(dev));
		}
		void clear() {
			devices.clear();
		}
		void remove(DEVICE* dev) {
			devices.erase(std::remove_if(devices.begin(), devices.end(),
				[dev](const std::unique_ptr<DEVICE>& d) { return d.get() == dev; }),
				devices.end());
		}
		void logAll(const str& msg) {
			for (auto& dev : devices) dev->Log(msg);
		}
		str toString() const {
			std::string result;
			for (const auto& dev : devices) {
				result += dev->toString() + " ";
			}
			return result.empty() ? "No Devices" : result;
		}

		// 禁止外部拷贝管理器，防止乱生命周期
		LogDevices() = default;
		~LogDevices() = default;
		LogDevices(const LogDevices&) = delete;
		LogDevices& operator=(const LogDevices&) = delete;
	};

	// 单个等级的简易接口
#define LOG_LEVEL_API(LevelName) \
template<typename... Args> \
void log##LevelName(std::string content, Args&&... args) { \
    this->template log<LevelName>( \
        std::forward<std::string>(content), \
        std::forward<Args>(args)... \
    ); \
}

// 批量生成所有等级的快捷接口 + 暴露两个通用模板接口
#define LOG_ALL_API \
    /* 生成所有等级的快捷调用 */ \
    LOG_LEVEL_API(Trace) \
    LOG_LEVEL_API(Debug) \
    LOG_LEVEL_API(Info) \
    LOG_LEVEL_API(Warn) \
    LOG_LEVEL_API(Error) \
    LOG_LEVEL_API(Fatal)

}

