#pragma once
#include "XCore/include/XYCore.h"
#include <functional>
#include <vector>
#include <sstream>
#include <queue>
#include <unordered_map>
#include "Log/include/XYLog.h"
namespace X_Y
{
	// 窗口发送消息->winproc转化成某种行为，放入消息队列--
	//  ->分发器从队列中取出某种行为，调用所有监听该行为的函数
	// 消息队列和分发器都是全局，程序所持有

	// 添加动作（程序行为,窗口行为，鼠标键盘行为），或者说系统事件
	// 类型固定
	// 设计希望窗口具有某些行为，这些行为将触发逻辑事件，
	enum MovementCategory
	{
		None = 0,
		MTApplication = BIT(0),
		MTInput = BIT(1),
		MTKeyboard = BIT(2),
		MTMouse = BIT(3),
		MTMouseButton = BIT(4),
		MTWindow = BIT(5)
	};
	enum class MovementType
	{
		None = 0,
		AppTick,
		AppUpdate,
		AppRender,
		WindowClose,
		WindowResize,
		WindowFocus,
		WindowMoved,
		WindowDestroy,
		WindowPaint,
		KeyPressed,
		KeyReleased,
		KeyTyped,
		MouseButtonPressed,
		MouseButtonReleased,
		MouseMoved,
		MouseScrolled,
		WindowDragBegin,
		WindowDragEnd

	};
#define MOVEMENT_CLASS_TYPE(type)                                             \
	static MovementType GetStaticType() { return MovementType::type; }        \
	virtual MovementType GetType() const override { return GetStaticType(); } \
	virtual const char *GetName() const override { return #type; }

#define MOVEMENT_CLASS_CATEGORY(category) \
	virtual int GetCategoryFlags() const override { return category; }

	using MovementSender = void *;	 // 发起者（窗口/任意对象）
	using MovementReceiver = void *; // 接收者（任意对象）
	struct XMovement
	{
		virtual ~XMovement() = default;
		XMovement(MovementSender s) : sender(s) {};
		virtual bool DispatchEvent(void *dispatcher) = 0;
		MovementSender sender = nullptr;
		bool Handled = false;
	};
	// 消息队列知道行为及其发起者
	// 分发器通过匹配消息队列的信息寻找监听者执行其回调
	// 所以分发器维护了全局监听表
	// 接下来写分发器
	// using MovementHandler = std::function<void(XMovement* event)>;
	using MovementHandler = std::function<void(const XMovement &)>;
	using EventType = uint32_t;
	using TypeID = uint32_t;
	struct MovementBinding
	{
		MovementSender sender;	   // 谁发的
		EventType type;			   // 什么行为
		TypeID typeId;			   // 什么类型
		MovementReceiver receiver; // 谁接收
		MovementHandler handler;   // 怎么处理
	};
	class MovementDispatcher
	{
	public:
		// -------------------------------------------------------------------------
		// 模板 Connect：自动识别枚举类型，存入同一个列表
		// -------------------------------------------------------------------------
		template <typename EnumT>
		void Connect(
			MovementSender sender,
			EnumT type,
			MovementReceiver receiver,
			MovementHandler handler)
		{
			XY_CORE_ASSERT(std::is_enum_v<EnumT>, "必须是枚举类型");
			XDEBUG("入队加一；{}", type)
			MovementBinding bind{};
			bind.sender = sender;
			bind.type = static_cast<EventType>(type);
			bind.typeId = GetTypeId<EnumT>(); // 自动区分枚举类型
			bind.receiver = receiver;
			bind.handler = std::move(handler);

			m_Bindings.push_back(std::move(bind)); // 全部存在一起
		}

		// -------------------------------------------------------------------------
		// 模板 Disconnect：按【枚举+类型+接收者】删除
		// -------------------------------------------------------------------------
		template <typename EnumT>
		void disConnect(
			MovementSender sender,
			EnumT type,
			MovementReceiver receiver)
		{
			XY_CORE_ASSERT(std::is_enum_v<EnumT>, "必须是枚举类型");

			TypeID typeId = GetTypeId<EnumT>();
			EventType typeVal = static_cast<EventType>(type);

			for (auto it = m_Bindings.begin(); it != m_Bindings.end();)
			{
				if (it->sender == sender &&
					it->typeId == typeId &&
					it->type == typeVal &&
					it->receiver == receiver)
				{
					it = m_Bindings.erase(it);
				}
				else
				{
					++it;
				}
			}
		}

		// -------------------------------------------------------------------------
		// 全局 Disconnect：按【sender + receiver】删除（你原来的）
		// -------------------------------------------------------------------------
		void disConnect(
			MovementSender sender,
			MovementReceiver receiver)
		{
			for (auto it = m_Bindings.begin(); it != m_Bindings.end();)
			{
				if (it->receiver == receiver && it->sender == sender)
				{
					XDEBUG("回调被删除");
					it = m_Bindings.erase(it);
				}
				else
				{
					++it;
				}
			}
		}

		// -------------------------------------------------------------------------
		// 全局 Disconnect：只按【receiver】删除（你原来的）
		// -------------------------------------------------------------------------
		void disConnect(MovementReceiver receiver)
		{
			for (auto it = m_Bindings.begin(); it != m_Bindings.end();)
			{
				if (it->receiver == receiver)
				{
					XDEBUG("回调被删除");
					it = m_Bindings.erase(it);
				}
				else
				{
					++it;
				}
			}
		}
		// -------------------------------------------------------------------------
		// ✅ Dispatch：接收事件，自动匹配【枚举类型+值】
		// -------------------------------------------------------------------------
		bool DispatchEvent(uint32_t typeId, uint32_t typeVal, XMovement *event)
		{
			if (!event)
				return false;
			bool handled = false;
			for (auto &bind : m_Bindings)
			{
				if (bind.sender == event->sender &&
					bind.typeId == typeId && // 严格匹配枚举类型
					bind.type == typeVal)	 // 严格匹配枚举值
				{
					bind.handler(*event);
					handled = true;
				}
			}

			event->Handled = handled;
			return handled;
		}
		template <typename EMT>
		bool Dispatch(EMT type, XMovement *event)
		{
			uint32_t typeId = GetTypeId<EMT>();
			uint32_t typeVal = static_cast<uint32_t>(type);
			//if (DispatchEvent(typeId, typeVal, event))
			//{
			//	return true;
			//}
			//else
			//{
			//	return false;
			//}
			 return DispatchEvent(typeId, typeVal, event);
		}

	private:
		// -------------------------------------------------------------------------
		// 每个枚举类型生成唯一ID
		// -------------------------------------------------------------------------
		inline static uint32_t g_TypeIdNext = 0;
		template <typename T>
		static uint32_t GetTypeId()
		{
			static const uint32_t id = g_TypeIdNext++;
			// XINFO("类型id{}",id)
			return id;
		}

	private:
		// 所有绑定存在同一个列表！
		std::vector<MovementBinding> m_Bindings;
	};

	// 全局消息队列
	class MovementQueue
	{
	public:
		// 窗口/发起者 把行为推入队列
		void Push(XMovement *event)
		{
			m_Queue.push(event);
		}
		// 取出一个行为
		XMovement *Pop()
		{
			if (m_Queue.empty())
				return nullptr;
			XMovement *e = m_Queue.front();
			m_Queue.pop();
			return e;
		}
		bool Empty() const { return m_Queue.empty(); }

	private:
		std::queue<XMovement *> m_Queue;
	};

	class Layer
	{
	public:
		virtual ~Layer() = default;

		// 层被挂载时调用
		virtual void OnAttach() {}

		// 层被卸载时调用
		virtual void OnDetach() {}

		// 每一帧更新
		virtual void OnUpdate() {}

		// 层接收事件
		virtual void OnEvent(XMovement *event) {}
	};

	class LayerStack
	{
	public:
		void PushLayer(Layer *layer)
		{
			m_Layers.push_back(layer);
			layer->OnAttach();
		}

		void PopLayer(Layer *layer)
		{
			auto it = std::find(m_Layers.begin(), m_Layers.end(), layer);
			if (it != m_Layers.end())
			{
				layer->OnDetach();
				m_Layers.erase(it);
			}
		}

		std::vector<Layer *> &GetLayers() { return m_Layers; }

	private:
		std::vector<Layer *> m_Layers;
	};
	// 窗口的行为类型，携带自己的消息
	// 窗口只负责将行为发送给消息队列
	class Movement : public XMovement
	{
	public:
		virtual ~Movement() = default;
		Movement(MovementSender s) : XMovement(s) {};
		virtual MovementType GetType() const = 0;
		virtual const char *GetName() const = 0;
		virtual int GetCategoryFlags() const = 0;
		virtual std::string toString() const
		{
			return GetName();
		};
		bool DispatchEvent(void *dispatcher) override
		{
			auto *d = static_cast<MovementDispatcher *>(dispatcher);
			bool flag = d->Dispatch(this->GetType(), this);
			if (this->GetType() == MovementType::WindowPaint)
			{
				return flag;
			}
			if (flag)
			{
				XDEBUG("{}事件处理成功{}", reinterpret_cast<uintptr_t>(this->sender),this->GetName())
			}
			else
			{
				XDEBUG("{}事件处理失败{}", reinterpret_cast<uintptr_t>(this->sender),this->GetName())
			}
			return flag;
		}
		bool IsInCategory(MovementCategory category)
		{
			return GetCategoryFlags() & category;
		}
		friend std::ostream &operator<<(std::ostream &os, const Movement &e)
		{
			return os << e.toString();
		}
	};
	using EventSender = MovementSender;		// 发起者（窗口/任意对象）
	using EventReceiver = MovementReceiver; // 接收者（任意对象）
	// 用于逻辑事件
	// 可以继续仿写队列和分发器
	class Event : public XMovement
	{
	public:
		bool Handled = false;
		EventSender sender = nullptr;
		virtual ~Event() = default;
		Event(EventSender s) : XMovement(s) {};
		virtual int GetCategoryFlags() const = 0;

		virtual const char *GetName() const = 0;
		virtual std::string toString() const
		{
			return GetName();
		};
		bool IsInCategory(MovementCategory category)
		{
			return GetCategoryFlags() & category;
		}
		friend std::ostream &operator<<(std::ostream &os, const Event &e)
		{
			return os << e.toString();
		}
	};
#define EVENT_DISPATHC_INIT                                      \
	bool DispatchEvent(void *dispatcher) override                \
	{                                                            \
		auto *d = static_cast<MovementDispatcher *>(dispatcher); \
		bool flag = d->Dispatch(this->GetType(), this);          \
		if (flag)                                                \
		{                                                        \
			XDEBUG("事件处理成功{}", this->GetName())            \
		}                                                        \
		else                                                     \
		{                                                        \
			XDEBUG("事件处理失败{}", this->GetName())            \
		}                                                        \
		return flag;                                             \
	}
}