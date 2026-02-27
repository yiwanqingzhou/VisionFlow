#pragma once
#include <GLFW/glfw3.h>

class UIManager {
public:
  static GLFWwindow *Init();
  static void Cleanup(GLFWwindow *window);
  static void StartFrame();
  static void EndFrame(GLFWwindow *window);
};
