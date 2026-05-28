#pragma once
#include"Log/src/XYCore.h"
#include<functional>
#include<vector>
#include<sstream>
#include<queue>
namespace X_Y {
	//窗口发送消息->winproc转化成某种行为，放入消息队列--
	// ->分发器从队列中取出某种行为，调用所有监听该行为的函数
	//消息队列和分发器都是全局，程序所持有


	//添加动作（程序行为,窗口行为，鼠标键盘行为），或者说系统事件
	//类型固定
	//设计希望窗口具有某些行为，这些行为将触发逻辑事件，
	enum MovementCategory
	{
		None=0,
		MTApplication = BIT(0),
		MTInput = BIT(1),
		MTKeyboard = BIT(2),
		MTMouse = BIT(3),
		MTMouseButton=BIT(4),
		MTWindow=BIT(5)
	};
	enum class MovementType {
		None=0,
		AppTick,AppUpdate,AppRender,
		WindowClose,WindowResize,WindowFocus,WindowMoved,WindowDestory,
		KeyPressed,KeyReleased,KeyTyped,
		MouseButtonPressed,MouseButtonReleased,MouseMoved,MouseScrolled

	};
#define MOVEMENT_CLASS_TYPE(type) static MovementType GetStaticType() { return MovementType::type; }\
								virtual MovementType GetType() const override { return GetStaticType(); }\
								virtual const char* GetName() const override { return #type; }

#define  MOVEMENT_CLASS_CATEGORY(category) virtual int GetCategoryFlags() const override { return category; }

	using MovementSender = void*; // 发起者（窗口/任意对象）
	using MovementReceiver = void*; // 接收者（任意对象）
	//窗口的行为类型，携带自己的消息
	//窗口只负责将行为发送给消息队列
	class Movement {
	public:
		virtual ~Movement() = default;
		bool Handled = false;
		MovementSender sender = nullptr;
		Movement(MovementSender s):sender(s){};
		virtual MovementType GetType() const = 0;
		virtual const char* GetName() const = 0;
		virtual int GetCategoryFlags() const = 0;
		virtual std::string toString()const {
			return GetName();
		};

		bool IsInCategory(MovementCategory category) {
			return GetCategoryFlags() & category;
		}
		friend std::ostream& operator<<(std::ostream& os, const Movement& e)
		{
			return os << e.toString();
		}
	};
	//消息队列知道行为及其发起者
	//分发器通过匹配消息队列的信息寻找监听者执行其回调
	//所以分发器维护了全局监听表
	//接下来写分发器
	//using MovementHandler = std::function<void(Movement* event)>;
	using MovementHandler = std::function<void()>;
	struct MovementBinding
	{
		MovementSender     sender;     // 谁发的
		MovementType       type;       // 什么行为
		MovementReceiver   receiver;   // 谁接收
		MovementHandler    handler;    // 怎么处理
	};
	class MovementDispatcher
	{
	public:
		void Connect(
			MovementSender sender,
			MovementType type,
			MovementReceiver receiver,
			MovementHandler handler
		);
		void disConnect(
			MovementSender sender,
			MovementType type,
			MovementReceiver receiver
		);
		void disConnect(
			MovementSender sender,
			MovementReceiver receiver);
		void disConnect(MovementReceiver receiver);
		// === 触发：发送行为，自动找到所有绑定并执行 ===
		bool DispatchEvent(Movement* event);
	private:
		std::vector<MovementBinding> m_Bindings;
	};
	//全局消息队列
	class MovementQueue {
	public:
		// 窗口/发起者 把行为推入队列
		void Push(Movement* event) {
			m_Queue.push(event);
		}
		// 取出一个行为
		Movement* Pop() {
			if (m_Queue.empty()) return nullptr;
			Movement* e = m_Queue.front();
			m_Queue.pop();
			return e;
		}
		bool Empty() const { return m_Queue.empty(); }
	private:
		std::queue<Movement*> m_Queue;
	};

	//用于逻辑事件
	//可以继续仿写队列和分发器
	class Event :public Movement {};
}