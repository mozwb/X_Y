#include"xypch.h"
#include<Log/include/XYLog.h>
#include<XCore/include/XYTools.h>
#include<Application/include/Application.h>
#include<UI/include/imgui/ImGuiLayer.h>
#include<Render/include/renderWin/renderWin.h>
#include<Render/include/RenderCommand.h>
#include<Render/include/renderMath/RenderMath.h>
#include<glad/glad.h>

// hardcoded shader sources
static const char* s_TriangleVertexSrc =
    "#version 330 core\n"
    "layout(location = 0) in vec3 a_Position;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(a_Position, 1.0);\n"
    "}\n";

static const char* s_TriangleFragmentSrc =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "    FragColor = vec4(0.8, 0.2, 0.3, 1.0);\n"
    "}\n";

// TriangleWin : use RenderWin to draw a triangle
class TriangleWin : public X_Y::RenderWin
{
public:
    TriangleWin()
        : X_Y::RenderWin(nullptr)
    {
        setTitle("Triangle Test - Render");
        setSize(800, 600);
    }

    void setup()
    {
        createContext<X_Y::OpenglGrContext>();

        X_Y::RenderStart* e = new X_Y::RenderStart(this,
            get_context(), get_width(), get_height());
        X_Y::Application::instance()->GetEventQueue().Push(e);
        X_Y::Application::instance()->ProcessEvents();

        // 窗口创建后才能拿到 HWND
        m_ImGuiLayer.SetHwnd(GetNativeWindow());

        m_Shader = X_Y::Shader::Create("Triangle",
            s_TriangleVertexSrc, s_TriangleFragmentSrc);

        float vertices[3][3] = {
            { -0.5f, -0.5f, 0.0f },
            {  0.5f, -0.5f, 0.0f },
            {  0.0f,  0.5f, 0.0f }
        };

        m_VertexBuffer = X_Y::VertexBuffer::Create(&vertices[0][0], sizeof(vertices));
        m_VertexBuffer->SetLayout({
            { X_Y::ShaderDataType::Float3, "a_Position" }
        });

        m_VertexArray = X_Y::VertexArray::Create();
        m_VertexArray->AddVertexBuffer(m_VertexBuffer);
    }

    X_Y::ImGuiLayer m_ImGuiLayer;
protected:
    void onRender() override
    {
        X_Y::RenderCommand::SetClearColor(
            X_Y::RenderMath::Vec4(0.1f, 0.1f, 0.2f, 1.0f));
        X_Y::RenderCommand::Clear();

        m_Shader->Bind();
        X_Y::RenderCommand::DrawArrays(m_VertexArray,3);

        // ImGui 渲染
        m_ImGuiLayer.End();
    }

    X_Y::Ref<X_Y::Shader> m_Shader;
    X_Y::Ref<X_Y::VertexArray> m_VertexArray;
    X_Y::Ref<X_Y::VertexBuffer> m_VertexBuffer;
};

int main(int argc, char* argv[])
{
    X_Y::Application app(argc, argv);

    TriangleWin win;
    win.show();
    win.setup();

    // ImGuiLayer 挂入层栈（自动 OnAttach 初始化）
    app.PushLayer(&win.m_ImGuiLayer);

    while (app.isRunning())
    {
        // ImGui 新帧
        app.ProcessEvents();
        win.m_ImGuiLayer.Begin();

        // 画 ImGui 控件
        ImGui::ShowDemoWindow();

        win.Render();
        app.pushEvents();
    }

    // Layer detach 时自动 OnDetach 清理 ImGui
    return 0;
}
