#include "ui/ui_manager.hpp"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb/stb_image.h"
#include "utils/logger.hpp"

GLFWwindow* UIManager::init() {
  if (!glfwInit()) return nullptr;

  const char* glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow* window =
      glfwCreateWindow(1550, 900, "TaskNodeFlow - Editor", NULL, NULL);
  if (!window) return nullptr;

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);  // Enable v-sync

  // Initialize ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // Load a higher resolution font instead of blurring via global scale
  ImFontConfig config;
  config.MergeMode = false;
  io.Fonts->AddFontFromFileTTF(
      "../third_party/imgui/misc/fonts/Karla-Regular.ttf", 18.0f, &config);

  // Add default font as fallback for icons (it contains basic shapes)
  config.MergeMode = true;
  static const ImWchar icon_ranges[] = {0x2000, 0x3000,
                                        0};  // General Punctuation and symbols
  io.Fonts->AddFontDefault(&config);

  ImGui::StyleColorsDark();

  // Initialize Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  return window;
}

void UIManager::start_frame() {
  glfwPollEvents();
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  // Clear background
  int display_w, display_h;
  glfwGetFramebufferSize(glfwGetCurrentContext(), &display_w, &display_h);
  glViewport(0, 0, display_w, display_h);
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void UIManager::end_frame(GLFWwindow* window) {
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  glfwSwapBuffers(window);
}

bool UIManager::LoadTextureFromFile(const char* filename, GLuint* out_texture,
                                    int* out_width, int* out_height) {
  // Load from file
  int image_width = 0;
  int image_height = 0;
  unsigned char* image_data =
      stbi_load(filename, &image_width, &image_height, NULL, 4);
  if (image_data == NULL) return false;

  // Create a OpenGL texture identifier
  GLuint image_texture;
  glGenTextures(1, &image_texture);
  glBindTexture(GL_TEXTURE_2D, image_texture);

  // Setup filtering parameters for display
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // This is required on WebGL for non power-of-two textures
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we
  // might need this glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
  // GL_CLAMP_TO_EDGE); // we might need this

  // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, image_data);
  stbi_image_free(image_data);

  *out_texture = image_texture;
  *out_width = image_width;
  *out_height = image_height;

  return true;
}

void UIManager::cleanup(GLFWwindow* window) {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
}
