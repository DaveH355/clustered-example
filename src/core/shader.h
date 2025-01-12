#pragma once

#include <cstddef>
#include <functional>
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
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <gldoc.hpp>
#include <filesystem>
#include <string_view>
#include <unordered_map>

// do not call the construtor in static duration haha
// will segfault before main
class Shader
{
private:
  void compile_shader(const char *code, unsigned int type,
                      unsigned int &shaderID,
                      const std::filesystem::path &filePath);
  void check_compile_errors(unsigned int program, const std::string &type,
                            const std::filesystem::path &filePath);

  std::string read_file_into_string(const std::filesystem::path &filePath);

public:
  unsigned int program;

  Shader() : program(0) {}
  Shader(const std::filesystem::path &vertexPath,
         const std::filesystem::path &fragmentPath);
  Shader(const std::filesystem::path &computePath);

  void use() const;
  // Uniform setters
  void set_bool(const char *name, bool value) const;
  void set_int(const char *name, int value) const;
  void set_uint(const char *name, unsigned int value) const;
  void set_float(const char *name, float value) const;
  void set_vec2(const char *name, const glm::vec2 &value) const;
  void set_ivec2(const char *name, const glm::ivec2 &value) const;
  void set_uvec2(const char *name, const glm::uvec2 &value) const;
  void set_vec3(const char *name, const glm::vec3 &value) const;
  void set_ivec3(const char *name, const glm::ivec3 &value) const;
  void set_uvec3(const char *name, const glm::uvec3 &value) const;
  void set_vec4(const char *name, const glm::vec4 &value) const;
  void set_ivec4(const char *name, const glm::ivec4 &value) const;
  void set_uvec4(const char *name, const glm::uvec4 &value) const;
  void set_mat2(const char *name, const glm::mat2 &mat) const;
  void set_mat3(const char *name, const glm::mat3 &mat) const;
  void set_mat4(const char *name, const glm::mat4 &mat) const;
  void set_mat2x3(const char *name, const glm::mat2x3 &mat) const;
  void set_mat3x2(const char *name, const glm::mat3x2 &mat) const;
  void set_mat2x4(const char *name, const glm::mat2x4 &mat) const;
  void set_mat4x2(const char *name, const glm::mat4x2 &mat) const;
  void set_mat3x4(const char *name, const glm::mat3x4 &mat) const;
  void set_mat4x3(const char *name, const glm::mat4x3 &mat) const;
};
