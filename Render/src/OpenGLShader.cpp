#include"Render/include/OpenGL/OpenGLShader.h"
#include<string>
#include<Log/include/XYLog.h>
#include <fstream>
#include <unordered_map>
#include <filesystem>
#include <glm/glm.hpp>

namespace X_Y {
#define HZ_PROFILE_FUNCTION();
	namespace Utils {

		static GLenum ShaderTypeFromString(const std::string& type)
		{
			if (type == "vertex")
				return GL_VERTEX_SHADER;
			if (type == "fragment" || type == "pixel")
				return GL_FRAGMENT_SHADER;

			XY_CORE_ASSERT(false, "Unknown shader type!");
			return 0;
		}

	}

	OpenGLShader::OpenGLShader(const std::string& filepath)
		: m_FilePath(filepath)
	{
		HZ_PROFILE_FUNCTION();

		std::string source = ReadFile(filepath);
		auto shaderSources = PreProcess(source);

		{
			Timer timer;
			Compile(shaderSources);
			XY_CORE_WARN("Shader creation took {0} ms", timer.ElapsedMillis());
		}

		// Extract name from filepath
		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind('.');
		auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
		m_Name = filepath.substr(lastSlash, count);
	}

	OpenGLShader::OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
		: m_Name(name)
	{
		HZ_PROFILE_FUNCTION();

		std::unordered_map<GLenum, std::string> sources;
		sources[GL_VERTEX_SHADER] = vertexSrc;
		sources[GL_FRAGMENT_SHADER] = fragmentSrc;

		Compile(sources);
	}

	OpenGLShader::~OpenGLShader()
	{
		HZ_PROFILE_FUNCTION();
		glDeleteProgram(m_RendererID);
	}

	std::string OpenGLShader::ReadFile(const std::string& filepath)
	{
		HZ_PROFILE_FUNCTION();

		std::string result;
		std::ifstream in(filepath, std::ios::in | std::ios::binary);
		if (in)
		{
			in.seekg(0, std::ios::end);
			size_t size = in.tellg();
			if (size != -1)
			{
				result.resize(size);
				in.seekg(0, std::ios::beg);
				in.read(&result[0], size);
			}
			else
			{
				XY_CORE_ERROR("Could not read from file '{0}'", filepath);
			}
		}
		else
		{
			XY_CORE_ERROR("Could not open file '{0}'", filepath);
		}

		return result;
	}

	std::unordered_map<GLenum, std::string> OpenGLShader::PreProcess(const std::string& source)
	{
		HZ_PROFILE_FUNCTION();

		std::unordered_map<GLenum, std::string> shaderSources;

		const char* typeToken = "#type";
		size_t typeTokenLength = strlen(typeToken);
		size_t pos = source.find(typeToken, 0);
		while (pos != std::string::npos)
		{
			size_t eol = source.find_first_of("\r\n", pos);
			XY_CORE_ASSERT(eol != std::string::npos, "Syntax error");
			size_t begin = pos + typeTokenLength + 1;
			std::string type = source.substr(begin, eol - begin);
			XY_CORE_ASSERT(Utils::ShaderTypeFromString(type), "Invalid shader type specified");

			size_t nextLinePos = source.find_first_not_of("\r\n", eol);
			XY_CORE_ASSERT(nextLinePos != std::string::npos, "Syntax error");
			pos = source.find(typeToken, nextLinePos);

			shaderSources[Utils::ShaderTypeFromString(type)] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
		}

		return shaderSources;
	}

	void OpenGLShader::Compile(const std::unordered_map<GLenum, std::string>& shaderSources)
	{
		GLuint program = glCreateProgram();
		std::vector<GLuint> shaderIDs;

		for (auto&& [stage, source] : shaderSources)
		{
			GLuint shaderID = glCreateShader(stage);
			const char* src = source.c_str();
			glShaderSource(shaderID, 1, &src, nullptr);
			glCompileShader(shaderID);

			// 编译错误检查
			GLint compiled;
			glGetShaderiv(shaderID, GL_COMPILE_STATUS, &compiled);
			if (compiled == GL_FALSE)
			{
				GLint maxLen;
				glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &maxLen);
				std::vector<GLchar> log(maxLen);
				glGetShaderInfoLog(shaderID, maxLen, &maxLen, log.data());
				XY_CORE_ERROR("Shader Compile Failed:\n{0}", log.data());
				glDeleteShader(shaderID);
				continue;
			}

			shaderIDs.push_back(shaderID);
			glAttachShader(program, shaderID);
		}

		// 链接程序
		glLinkProgram(program);
		GLint linked;
		glGetProgramiv(program, GL_LINK_STATUS, &linked);
		if (linked == GL_FALSE)
		{
			GLint maxLen;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLen);
			std::vector<GLchar> log(maxLen);
			glGetProgramInfoLog(program, maxLen, &maxLen, log.data());
			XY_CORE_ERROR("Shader Link Failed ({0}):\n{1}", m_FilePath, log.data());

			glDeleteProgram(program);
			for (auto id : shaderIDs)
				glDeleteShader(id);
			return;
		}

		// 解绑并删除着色器对象
		for (auto id : shaderIDs)
		{
			glDetachShader(program, id);
			glDeleteShader(id);
		}

		m_RendererID = program;
	}

	void OpenGLShader::Bind() const
	{
		HZ_PROFILE_FUNCTION();
		glUseProgram(m_RendererID);
	}

	void OpenGLShader::Unbind() const
	{
		HZ_PROFILE_FUNCTION();
		glUseProgram(0);
	}

	void OpenGLShader::SetInt(const std::string& name, int value)
	{
		HZ_PROFILE_FUNCTION();
		UploadUniformInt(name, value);
	}

	void OpenGLShader::SetIntArray(const std::string& name, int* values, uint32_t count)
	{
		HZ_PROFILE_FUNCTION();
		UploadUniformIntArray(name, values, count);
	}

	void OpenGLShader::SetFloat(const std::string& name, float value)
	{
		HZ_PROFILE_FUNCTION();
		UploadUniformFloat(name, value);
	}

	void OpenGLShader::SetFloat2(const std::string& name, const glm::vec2& value)
	{
		HZ_PROFILE_FUNCTION();
		UploadUniformFloat2(name, value);
	}

	void OpenGLShader::SetFloat3(const std::string& name, const glm::vec3& value)
	{
		HZ_PROFILE_FUNCTION();
		UploadUniformFloat3(name, value);
	}

	void OpenGLShader::SetFloat4(const std::string& name, const glm::vec4& value)
	{
		HZ_PROFILE_FUNCTION();
		UploadUniformFloat4(name, value);
	}

	void OpenGLShader::SetMat4(const std::string& name, const glm::mat4& value)
	{
		HZ_PROFILE_FUNCTION();
		UploadUniformMat4(name, value);
	}

	void OpenGLShader::UploadUniformInt(const std::string& name, int value)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1i(location, value);
	}

	void OpenGLShader::UploadUniformIntArray(const std::string& name, int* values, uint32_t count)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1iv(location, count, values);
	}

	void OpenGLShader::UploadUniformFloat(const std::string& name, float value)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1f(location, value);
	}

	void OpenGLShader::UploadUniformFloat2(const std::string& name, const glm::vec2& value)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform2f(location, value.x, value.y);
	}

	void OpenGLShader::UploadUniformFloat3(const std::string& name, const glm::vec3& value)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform3f(location, value.x, value.y, value.z);
	}

	void OpenGLShader::UploadUniformFloat4(const std::string& name, const glm::vec4& value)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform4f(location, value.x, value.y, value.z, value.w);
	}

	void OpenGLShader::UploadUniformMat3(const std::string& name, const glm::mat3& matrix)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniformMatrix3fv(location, 1, GL_FALSE, RenderMath::Value_ptr(matrix));
	}

	void OpenGLShader::UploadUniformMat4(const std::string& name, const glm::mat4& matrix)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniformMatrix4fv(location, 1, GL_FALSE, RenderMath::Value_ptr(matrix));
	}
}