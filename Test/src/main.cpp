#include"xypch.h"
#include<Log/include/XYLog.h>
#include"Window/include/XWidget.h"
#include"Application/include/Application.h"
#include"GraphicsContext/include/GraphicsContext.h"
#include"glad/glad.h"

class GLWin : public X_Y::XWidget {
private:
    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;
    unsigned int m_ShaderProgram = 0;
    bool m_initialized = false; // 初始化标志
    bool m_gladInitialized = false;

    const float m_TriangleVertices[18] = {
        0.0f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f,
       -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,
        0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f
    };

    unsigned int CompileShader(unsigned int type, const char* source) {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        return shader;
    }

    void CreateShader() {
        const char* vertexShader = R"(
            #version 330 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec3 aColor;
            out vec3 vertexColor;
            void main(){
                gl_Position = vec4(aPos, 1.0);
                vertexColor = aColor;
            }
        )";

        const char* fragmentShader = R"(
            #version 330 core
            in vec3 vertexColor;
            out vec4 FragColor;
            void main(){
                FragColor = vec4(vertexColor, 1.0);
            }
        )";

        unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexShader);
        unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShader);

        m_ShaderProgram = glCreateProgram();
        glAttachShader(m_ShaderProgram, vs);
        glAttachShader(m_ShaderProgram, fs);
        glLinkProgram(m_ShaderProgram);

        glDeleteShader(vs);
        glDeleteShader(fs);
    }

    void InitTriangle() {
        // 这里调用 glad 函数时，上下文必须已经绑定！
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);

        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(m_TriangleVertices), m_TriangleVertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        CreateShader();
    }

public:
    GLWin() {
 }
    void onRender() override {
        if (!m_gladInitialized) {
            if (!gladLoadGL()) {
                MessageBoxA(NULL, "glad 初始化失败", "错误", 0);
                return;
            }
            m_gladInitialized = true;
        }
        // 只在第一次渲染时初始化三角形
        if (!m_initialized) {
            InitTriangle();
            m_initialized = true;
        }
        // 清屏 + 绘制
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(m_ShaderProgram);
        glBindVertexArray(m_VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    ~GLWin() {
        if (m_initialized) {
            glDeleteVertexArrays(1, &m_VAO);
            glDeleteBuffers(1, &m_VBO);
            glDeleteProgram(m_ShaderProgram);
        }
    }
};

int main(int argc, char* argv[]) {
    X_Y::Application app(argc, argv);

    // ====================== 替换成你自己的 GLWin 窗口 ======================
    GLWin win;
    win.setTitle("OpenGL 三角形");
    win.show();
    win.createContext<X_Y::OpenglGrContext>();
    while (app.isRunning()) {
        app.pushEvents();
        app.ProcessEvents();
        win.Render();  // 自动调用 onRender
    }

    return 0;
}