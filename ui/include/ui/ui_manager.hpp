#pragma once
#include <GLFW/glfw3.h>

class UIManager {
 public:
  static GLFWwindow* init();
  static void cleanup(GLFWwindow* window);
  static void start_frame();
  static void end_frame(GLFWwindow* window);
  static bool LoadTextureFromFile(const char* filename, GLuint* out_texture,
                                  int* out_width, int* out_height);
};
