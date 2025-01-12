#include "shader.h"
#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <glm/ext/matrix_float2x2.hpp>
#include <glm/ext/matrix_float2x3.hpp>
#include <glm/ext/matrix_float2x4.hpp>
#include <glm/ext/matrix_float3x2.hpp>
#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_float3x4.hpp>
#include <glm/ext/matrix_float4x2.hpp>
#include <glm/ext/matrix_float4x3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/vector_int2.hpp>
#include <glm/ext/vector_int3.hpp>
#include <glm/ext/vector_int4.hpp>
#include <glm/ext/vector_uint2.hpp>
#include <glm/ext/vector_uint3.hpp>
#include <glm/ext/vector_uint4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <iostream>
#include <gldoc.hpp>
#include <stdexcept>
#include <string>

Shader::Shader(const std::filesystem::path &vertexPath,
               const std::filesystem::path &fragmentPath)
{
  std::string vertCode = read_file_into_string(vertexPath);
  std::string fragCode = read_file_into_string(fragmentPath);

  unsigned int vertShader, fragShader;
  compile_shader(vertCode.c_str(), GL_VERTEX_SHADER, vertShader, vertexPath);
  compile_shader(fragCode.c_str(), GL_FRAGMENT_SHADER, fragShader,
                 fragmentPath);

  program = glCreateProgram();
  std::cout << vertexPath.filename().c_str() << ", "
            << fragmentPath.filename().c_str() << " id: " << program << "\n";

  glAttachShader(program, vertShader);
  glAttachShader(program, fragShader);
  glLinkProgram(program);

  check_compile_errors(program, "PROGRAM", "");

  glDeleteShader(vertShader);
  glDeleteShader(fragShader);
}

Shader::Shader(const std::filesystem::path &computePath)
{
  std::string computeCode = read_file_into_string(computePath);

  unsigned int computeShader;
  compile_shader(computeCode.c_str(), GL_COMPUTE_SHADER, computeShader,
                 computePath);

  program = glCreateProgram();
  std::cout << computePath.filename().c_str() << " id: " << program << "\n";
  glAttachShader(program, computeShader);
  glLinkProgram(program);

  check_compile_errors(program, "PROGRAM", "");

  glDeleteShader(computeShader);
}

void Shader::compile_shader(const char *code, GLenum type,
                            unsigned int &shaderID,
                            const std::filesystem::path &filePath)
{
  shaderID = glCreateShader(type);
  glShaderSource(shaderID, 1, &code, nullptr);
  glCompileShader(shaderID);

  check_compile_errors(
      shaderID,
      type == GL_VERTEX_SHADER
          ? "VERTEX"
          : (type == GL_FRAGMENT_SHADER ? "FRAGMENT" : "COMPUTE"),
      filePath);
}

void Shader::check_compile_errors(unsigned int program, const std::string &type,
                                  const std::filesystem::path &filePath)
{
  int success;
  constexpr int LOG_SIZE = 1024;
  std::array<char, LOG_SIZE> infoLog{};
  if (type != "PROGRAM") // check shader compile errors
  {
    glGetShaderiv(program, GL_COMPILE_STATUS, &success);
    if (!success)
    {
      glGetShaderInfoLog(program, LOG_SIZE, nullptr, infoLog.data());

      std::string log(infoLog.begin(), infoLog.end());
      throw std::runtime_error("SHADER_COMPILATION_ERROR\n"
                               "Type: " +
                               type + "\nFile: " + filePath.string() + "\n" +
                               log);
    }
  }
  else // check linking errors
  {
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
      glGetProgramInfoLog(program, LOG_SIZE, nullptr, infoLog.data());
      std::string log(infoLog.begin(), infoLog.end());

      throw std::runtime_error("SHADER_PROGRAM_LINKING_ERROR\n"
                               "Type: " +
                               type + "\n" + log);
    }
  }
}

std::string Shader::read_file_into_string(const std::filesystem::path &filePath)
{
  std::ifstream file(filePath);
  if (!file)
  {
    throw std::runtime_error("Could not open file: " + filePath.string());
  }
  std::stringstream stream;
  stream << file.rdbuf();
  return stream.str();
}

void Shader::use() const { glUseProgram(program); }

void Shader::set_bool(const char *name, bool value) const
{
  glUniform1i(glGetUniformLocation(program, name), (int)value);
}

void Shader::set_int(const char *name, int value) const
{
  glUniform1i(glGetUniformLocation(program, name), value);
}

void Shader::set_uint(const char *name, unsigned int value) const
{
  glUniform1ui(glGetUniformLocation(program, name), value);
}

void Shader::set_float(const char *name, float value) const
{
  glUniform1f(glGetUniformLocation(program, name), value);
}

void Shader::set_vec2(const char *name, const glm::vec2 &value) const
{
  glUniform2fv(glGetUniformLocation(program, name), 1, glm::value_ptr(value));
}

void Shader::set_ivec2(const char *name, const glm::ivec2 &value) const
{
  glUniform2iv(glGetUniformLocation(program, name), 1, glm::value_ptr(value));
}

void Shader::set_uvec2(const char *name, const glm::uvec2 &value) const
{
  glUniform2uiv(glGetUniformLocation(program, name), 1, glm::value_ptr(value));
}

void Shader::set_vec3(const char *name, const glm::vec3 &value) const
{
  glUniform3fv(glGetUniformLocation(program, name), 1, glm::value_ptr(value));
}

void Shader::set_ivec3(const char *name, const glm::ivec3 &value) const
{
  glUniform3iv(glGetUniformLocation(program, name), 1, glm::value_ptr(value));
}

void Shader::set_uvec3(const char *name, const glm::uvec3 &value) const
{
  glUniform3uiv(glGetUniformLocation(program, name), 1, glm::value_ptr(value));
}

void Shader::set_vec4(const char *name, const glm::vec4 &value) const
{
  glUniform4fv(glGetUniformLocation(program, name), 1, glm::value_ptr(value));
}

void Shader::set_ivec4(const char *name, const glm::ivec4 &value) const
{
  glUniform4iv(glGetUniformLocation(program, name), 1, glm::value_ptr(value));
}

void Shader::set_uvec4(const char *name, const glm::uvec4 &value) const
{
  glUniform4uiv(glGetUniformLocation(program, name), 1, glm::value_ptr(value));
}

void Shader::set_mat2(const char *name, const glm::mat2 &mat) const
{
  glUniformMatrix2fv(glGetUniformLocation(program, name), 1, GL_FALSE,
                     glm::value_ptr(mat));
}

void Shader::set_mat3(const char *name, const glm::mat3 &mat) const
{
  glUniformMatrix3fv(glGetUniformLocation(program, name), 1, GL_FALSE,
                     glm::value_ptr(mat));
}

void Shader::set_mat4(const char *name, const glm::mat4 &mat) const
{
  glUniformMatrix4fv(glGetUniformLocation(program, name), 1, GL_FALSE,
                     glm::value_ptr(mat));
}

void Shader::set_mat2x3(const char *name, const glm::mat2x3 &mat) const
{
  glUniformMatrix2x3fv(glGetUniformLocation(program, name), 1, GL_FALSE,
                       glm::value_ptr(mat));
}

void Shader::set_mat3x2(const char *name, const glm::mat3x2 &mat) const
{
  glUniformMatrix3x2fv(glGetUniformLocation(program, name), 1, GL_FALSE,
                       glm::value_ptr(mat));
}

void Shader::set_mat2x4(const char *name, const glm::mat2x4 &mat) const
{
  glUniformMatrix2x4fv(glGetUniformLocation(program, name), 1, GL_FALSE,
                       glm::value_ptr(mat));
}

void Shader::set_mat4x2(const char *name, const glm::mat4x2 &mat) const
{
  glUniformMatrix4x2fv(glGetUniformLocation(program, name), 1, GL_FALSE,
                       glm::value_ptr(mat));
}

void Shader::set_mat3x4(const char *name, const glm::mat3x4 &mat) const
{
  glUniformMatrix3x4fv(glGetUniformLocation(program, name), 1, GL_FALSE,
                       glm::value_ptr(mat));
}

void Shader::set_mat4x3(const char *name, const glm::mat4x3 &mat) const
{
  glUniformMatrix4x3fv(glGetUniformLocation(program, name), 1, GL_FALSE,
                       glm::value_ptr(mat));
}
