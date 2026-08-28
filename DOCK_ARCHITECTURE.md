# Dock 架构设计文档

## 概述

新的Dock架构采用抽象基类设计，将核心布局管理与具体窗口管理分离，提供更好的扩展性。

## 架构设计

### 1. Dock - 抽象基类

**职责：**
- 布局边界管理
- Split/Merge操作
- 血缘关系管理
- 布局计算

**纯虚函数（必须实现）：**
```cpp
virtual Dock* Split(Direction dir, float size) = 0;
virtual void Merge() = 0;
```

**虚函数（可选重写）：**
```cpp
// 窗口管理
virtual bool CanAcceptWindowDrag(int x, int y) const;
virtual bool OnWindowDropped(XWidget* window, int x, int y);
virtual bool CanCloseWindow() const;
virtual bool CloseWindow();
virtual XWidget* GetActiveWindow() const;

// 生命周期
virtual void OnAddedToLayout(DockLayout* layout);
virtual void OnRemovedFromLayout();
```

### 2. PanelDock - 具体实现

**功能：**
- Container tab管理
- 窗口拖入检测和处理
- 窗口关闭功能
- 菜单栏交互

**使用场景：**
- 需要完整窗口管理功能的停靠区域
- 支持拖入外部窗口
- 支持tab切换和关闭

### 3. SimpleDock - 简单实现（可选）

可以创建一个简化版本，只支持基本的布局管理，不支持窗口拖入等复杂功能。

## 使用示例

### 1. 创建带窗口管理的Dock

```cpp
// 创建主布局
auto* dockLayout = new DockLayout(parent);

// 创建支持窗口管理的PanelDock
auto* mainDock = new PanelDock(dockLayout);
dockLayout->AddDock(mainDock);
dockLayout->DockBind(*mainDock, InvalidBoundary, InvalidBoundary, 
                     InvalidBoundary, InvalidBoundary);

// 添加容器
auto* container = new Container(mainDock);
auto* label = new Label("Content", container);
mainDock->AddContainer(container, "Tab 1");
```

### 2. 自定义Dock实现

```cpp
class MyCustomDock : public Dock
{
public:
    MyCustomDock(XWidget* parent = nullptr) : Dock(parent) {}
    
    Dock* Split(Direction dir, float size) override
    {
        // 实现Split逻辑
        auto* newDock = CreateNewDock(dir);
        if (newDock)
        {
            // 设置新Dock的属性
            // ...
        }
        return newDock;
    }
    
    void Merge() override
    {
        // 实现Merge逻辑
        PerformMerge();
    }
    
    // 重写其他需要的虚函数
    bool CanAcceptWindowDrag(int x, int y) const override
    {
        // 自定义拖入检测逻辑
        return true;
    }
};
```

### 3. 测试Split/Merge

```cpp
// Split - 创建新Dock
auto* newDock = mainDock->Split(PanelDock::Direction::Right, 0.3f);
if (newDock)
{
    auto* newPanelDock = dynamic_cast<PanelDock*>(newDock);
    if (newPanelDock)
    {
        // 向新Dock添加内容
        auto* container = new Container(newPanelDock);
        newPanelDock->AddContainer(container, "New Tab");
    }
}

// Merge - 关闭tab后自动触发
// 当PanelDock中的最后一个容器被关闭时，会自动调用Merge()
```

## 关键特性

### 1. ID稳定性
- 使用"墓碑稳定id"方案，删除boundary后id不前移
- 避免引用失效问题

### 2. 所有权管理
- DockLayout管理所有Dock的生命周期
- 具体窗口的所有权由调用方负责

### 3. 扩展性
- 纯虚函数定义核心接口
- 虚函数提供默认实现
- 子类可以选择性重写

### 4. 自动化
- 变空自动合并
- 拖入检测和处理
- 布局自动重排

## 测试

运行 `DockTestWindow` 查看完整功能演示：
- 拖动分割线调整大小
- 关闭tab测试自动合并
- 拖入窗口测试（需要实现拖入逻辑）

## 后续扩展

1. **SimpleDock** - 创建简化版本，不支持窗口管理
2. **TabBar** - 实现完整的标签栏绘制和交互
3. **DragDrop** - 完善窗口拖入功能
4. **Serialization** - 支持布局序列化和恢复
5. **MultipleLayouts** - 支持多个DockLayout并存