#pragma once
// 配置里用到的类型
#include<iostream>
#include<string>
#include <chrono>
#include <ctime>
#include<ostream>
#include<fstream>
#include"LogTools.h"
namespace X_Y {

	//这个里面是基础配置，该暴露在外面的都在外面了，拓展LEVEL用LOG_LEVEL_EXT,加LOG_LEVEL_API
	//拓展设备直接继承DEVICE，重写toString和Log方法，添加成员变量（如果需要的话），然后add到LogConfigure里就行了
	namespace LogConfigure {
		using namespace  LogTools;
		struct TIME :public  CustomLogBase {
			static std::string now() {
				auto now = std::chrono::system_clock::now();
				std::time_t t = std::chrono::system_clock::to_time_t(now);
				char buf[32];
				std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
				return buf;
			}

			std::string toString() const {
				return now();
			}
		};
		struct DATE :public  CustomLogBase {
			static std::string now() {
				auto now = std::chrono::system_clock::now();
				std::time_t t = std::chrono::system_clock::to_time_t(now);
				char buf[32];
				std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
				return buf;
			}

			// 支持 toString（给你之前的自动替换用）
			std::string toString() const {
				return now(); // 直接返回当前时间
			}
		};


		template<typename LEVEL_TYPE, typename SELF_TYPE>
		struct LEVEL :public CustomLogBase {
			LEVEL() = delete;
			static std::string& model()
			{
				static std::string m;
				return m;
			}
			static void setModel(std::string m) { model() = m; }
			static std::string getModel() { return model(); }
			static std::string toString() { return "LEVEL"; }
		};
		//template<typename LEVEL_TYPE, typename SELF_TYPE> std::string LEVEL<LEVEL_TYPE, SELF_TYPE>::model;
#define LOG_LEVEL_ALL \
public: \
    struct Trace : LEVEL<Trace, Self> { \
       static std::string toString() { return "TRACE"; } \
    }; \
    struct Debug : LEVEL<Debug, Self> { \
        static std::string toString() { return "DEBUG"; } \
    }; \
    struct Info : LEVEL<Info, Self> { \
        static std::string toString() { return "INFO"; } \
    }; \
    struct Warn : LEVEL<Warn, Self> { \
        static std::string toString() { return "WARN"; } \
    }; \
    struct Error : LEVEL<Error, Self> { \
        static std::string toString() { return "ERROR"; } \
    }; \
    struct Fatal : LEVEL<Fatal, Self> { \
        static std::string toString() { return "FATAL"; } \
    };

#define LOG_LEVEL_EXT(LevelName) \
	struct LevelName : LEVEL<LevelName, Self> { \
		static std::string toString() { return #LevelName; } \
	};

		struct DEVICE :public CustomLogBase {
			std::ostream* os = &std::cout;
			DEVICE() = default;
			DEVICE(std::ostream& os) :os(&os) {}
			//或许拓展要写
			virtual ~DEVICE() = default;
			virtual std::string toString() const {
				return	"DEVICE";
			};
			virtual void Log(const std::string& message)const {
				*os << message << std::endl;
			}
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
			std::string toString() const  override { return file.is_open() ? _filename : "文件打开失败"; }
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
			void logAll(const std::string& msg) {
				for (auto& dev : devices) dev->Log(msg);
			}
			std::string toString() const {
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
#define UN_KNOW "未知"
		template<typename T>
		class LogConfigure :public LogDevices {
		public:

			using str = std::string;
			template<typename T> using vec = std::vector<T>;
			using Self = T;
			using LogDevices::logAll;
			using LogDevices::add;
			using LogDevices::remove;
			using LogDevices::clear;
			using LogDevices::toString;
			template<typename A, typename B>
			using LEVEL = ::X_Y::LogConfigure::LEVEL<A, B>;
			
			
			
			LOG_LEVEL_ALL
			LOG_ALL_API


				LogConfigure() {
				name = "DefaultLogger";
				this->add(new DEVICE());
				Trace::setModel("%180:180:180%[位置][日期][时间][发起者][等级]:[内容]%#");
				Debug::setModel("%90:180:255%[位置][日期][时间][发起者][等级]:[内容]%#");
				Info::setModel("%80:220:100%[位置][日期][时间][发起者][等级]:[内容]%#");
				Warn::setModel("%255:210:0%[位置][日期][时间][发起者][等级]:[内容]%#");
				Error::setModel("%255:80:80%[位置][日期][时间][发起者][等级]:[内容]%#");
				Fatal::setModel("%220:0:0%[位置][日期][时间][发起者][等级]:[内容]%#");

			}
			LogConfigure(str name)
				: name(name)
			{
				this->add(new DEVICE());
				Trace::setModel("%180:180:180%[位置][日期][时间][发起者][等级]:[内容]%#");
				Debug::setModel("%90:180:255%[位置][日期][时间][发起者][等级]:[内容]%#");
				Info::setModel("%80:220:100%[位置][日期][时间][发起者][等级]:[内容]%#");
				Warn::setModel("%255:210:0%[位置][日期][时间][发起者][等级]:[内容]%#");
				Error::setModel("%255:80:80%[位置][日期][时间][发起者][等级]:[内容]%#");
				Fatal::setModel("%220:0:0%[位置][日期][时间][发起者][等级]:[内容]%#");
			}
			virtual ~LogConfigure() {};
			void setname(str name) { this->name = name; }
			std::string toString() const {
				std::ostringstream oss;
				oss << "日志发起者: " << name << "\n";
				oss << "链接输出设备:" << LogDevices::toString();
				return oss.str();
			}
			template<typename LevelType, typename T, typename... Args>
			void log(T&& content, Args&&... args) {
				// 内部直接调用私有版本，传默认值
				Log<LevelType>(nullptr, -1, nullptr,
					std::forward<T>(content),
					std::forward<Args>(args)...);
			}
			template<typename LevelType, typename T, typename... Args>
			void Log(const char* file, int line, const char* func, T&& content, Args&&... args) {
				str lev = LevelType::toString();
				str model = LevelType::model();
				str result = replace(content, args...);
				str Mfile = file == nullptr ? UN_KNOW : To_Str(file);
				str MLine = line == -1 ? UN_KNOW : To_Str(line);
				str Mfunc = func == nullptr ? UN_KNOW : To_Str(func);

				result = format(model, lev, result, Mfile, MLine, Mfunc);
				logAll(result);
			}
		private:
			template<typename T, typename... Args>
			str replace(T&& content, Args&&... args) {
				std::string s = To_Str(std::forward<T>(content));
				vec<str> args_vec;
				//逗号运算符你敢信，它能依次执行，结果是最后表达式结果
				(args_vec.push_back(To_Str(std::forward<Args>(args))), ...);
				s = PiPei(s, args_vec);
				return s;
			}
			//如果你不喜欢这个格式化方式，可以重写这个函数，匹配你想要的模板（model）,可以用regex库
			virtual	str format(str mod, str lev, str content, str file, str line, str func) {
				replaceModel(mod, "[位置]", "[" + file + ":" + line + ":" + func + "]");
				replaceModel(mod, "[日期]", "[" + To_Str(DATE()) + "]");
				replaceModel(mod, "[时间]", "[" + To_Str(TIME()) + "]");
				replaceModel(mod, "[发起者]", "[" + name + "]");
				replaceModel(mod, "[等级]", "[" + lev + "]");
				replaceModel(mod, "[内容]", content);
				replaceColor(mod);
				return mod;
			}


			//如果你不喜欢用{}匹配参数，可以重写这个函数，可以用regex库
			virtual  str PiPei(str s, vec<str> args) {
				for (auto arg : args) {
					replaceModel(s, '{', '}', arg);
				}
				replaceModel(s, "{{", "{");
				replaceModel(s, "}}", '}');
				return s;
			}

			friend std::ostream& operator<<(std::ostream& os, const LogConfigure<T>& log) {
				os << To_Str(log);
				return os;
			}

		public:
			str name;
		};
	}
}

