# UI

# 目录
- [UI](#ui)
- [目录](#目录)
- [总述](#总述)
- [Container](#container)
- [dock](#dock)
- [composite](#composite)
- [Component](#component)
- [总结](#总结)


---
# 总述

- container 是承载组件的容器
- dock 继承自container，给container布局的面板
- componet UI的最小工作组件
- composite 提供一个已经比较成熟可使用的容器

# Container 

- 因为Container 继承自XWidget，所以其本质就是一个窗口，但是职责区别于窗口那层。

- 对于窗口层来说:
  1. Application负责程序进程的循环(内部的PlatformLoop负责与Win32循环相关的内容，有关窗口类注册也在这里面)
  2. Basewin负责收束操作系统与窗口相关的职能(其中Canvas负责绘制的相关功能，Font为canvas提供写文字的能力，DPI负责适配屏幕缩放)
  3. XWidget其实有些鸡肋，这层几乎仅仅就是创建了一个窗口，但是它屏蔽了底层实现
  4. Movement我们的全局事件系统(分两层，layer优先级较高会先消费，之后是dispathcer，dispatcher目前是几乎负责了所有事件转发)
- 所以对于到此位置的事件链就是
```
    Windows 消息循环
        ↓
    Win32WndProc
        ↓
    生成 Movement
        ↓
    Application::ProcessEvents()
        ↓
    Layer 优先处理
        ↓
    未消费时进入 MovementDispatcher
        ↓
    Connect 注册的回调
```

- 而container主要是负责两个事情，一个是拓展Xwidget的功能比如画组件，另一个就是它在这里收束事件再次调度，就是收揽Movement然后分发给自己的组件，然后由组件直接触发自己的回调函数，另外因为它是窗口，所以它的OnPaint会在Win32WndProc消息循环里直接处理，所以它负责提供canvas给它的组件进行绘制，<span style="color:#f44336;">void OnPaint(Canvas* canvas)</span>同时它为组件提供一个回调，让组件主动请求重绘,在添加组件时注入
  
```cpp
    void Container::AddComponent(Component *comp)
    {
        if (comp)
        {
            // 注入重绘回调：组件 RequestRepaint() → 请求所属窗口重绘
            comp->SetRepaintCallback([this]()
                                     { RequestRepaint(); });
            m_Components.push_back(comp);
        }
    }
```
所以事件链就多了一层，到了container在分发全局事件之后，理论来讲就是组件内部的事件系统（但是由于目前还没用上，所以就还是普通的传参调用），这样子避免内部组件树的事件污染全局事件系统，也方便事件管理

# dock

- 本质就是想实现悬浮面板，让多个composite(实质是container)可以在一个dock里自由布局组合
  
# composite 

- 其实就是实现一个小功能的窗口程序，本质继承与Container


# Component

- 提供一些简单的功能组件
- 例如功能相关的按钮，文字窗口listbox,textinput,overlay悬浮预览之类
- 和布局组件布局相关的horizontal和vertical
  
- 然后由container做hittest然后调用component相关的dispatcher函数，(目前还没有实现对dispatcher的统一所以只有分散的函数)，然后又dispathcer内部做决定是直接调用自己的回调函数，还是继续转发给自己内部的组件

# 总结

- 这样设计感觉Ui内部就像是一个小黑箱子，因为它几乎不影响其他模块的内容，写好的成品直接放在composite，然后我们只需要内部进行迭代，最多影响的就是某些composite，（感觉dock也具有一个成熟的composite的意味，只不过没有放在composite文件下而已）