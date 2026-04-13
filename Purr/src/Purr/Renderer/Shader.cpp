#include "purrpch.h"
#include "Shader.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace Purr {

    static GLuint CompileShader(GLenum type, const std::string& src)
    {
        GLuint shader = glCreateShader(type);
        const char* s = src.c_str();
        glShaderSource(shader, 1, &s, nullptr);
        glCompileShader(shader);

        int ok;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetShaderInfoLog(shader, 512, nullptr, log);
            PURR_CORE_ERROR("Shader compile error: {0}", log);
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc)
    {
        GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSrc);
        GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);

        m_RendererID = glCreateProgram();
        glAttachShader(m_RendererID, vs);
        glAttachShader(m_RendererID, fs);
        glLinkProgram(m_RendererID);

        int ok;
        glGetProgramiv(m_RendererID, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetProgramInfoLog(m_RendererID, 512, nullptr, log);
            PURR_CORE_ERROR("Shader link error: {0}", log);
        }

        glDeleteShader(vs);
        glDeleteShader(fs);
    }

    Shader::~Shader() { glDeleteProgram(m_RendererID); }
    void Shader::Bind()   const { glUseProgram(m_RendererID); }
    void Shader::Unbind() const { glUseProgram(0); }

    int Shader::GetUniformLocation(const std::string& name)
    {
        auto it = m_UniformLocationCache.find(name);
        if (it != m_UniformLocationCache.end()) return it->second;
        int loc = glGetUniformLocation(m_RendererID, name.c_str());
        if (loc == -1) PURR_CORE_WARN("Uniform '{0}' not found", name);
        m_UniformLocationCache[name] = loc;
        return loc;
    }

    void Shader::SetInt(const std::string& n, int v) { glUniform1i(GetUniformLocation(n), v); }
    void Shader::SetFloat(const std::string& n, float v) { glUniform1f(GetUniformLocation(n), v); }
    void Shader::SetFloat3(const std::string& n, const glm::vec3& v) { glUniform3fv(GetUniformLocation(n), 1, glm::value_ptr(v)); }
    void Shader::SetFloat4(const std::string& n, const glm::vec4& v) { glUniform4fv(GetUniformLocation(n), 1, glm::value_ptr(v)); }
    void Shader::SetMat4(const std::string& n, const glm::mat4& v) { glUniformMatrix4fv(GetUniformLocation(n), 1, GL_FALSE, glm::value_ptr(v)); }

}