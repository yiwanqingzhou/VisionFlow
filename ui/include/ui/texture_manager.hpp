#pragma once

#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include <mutex>
#include <opencv2/opencv.hpp>
#include <string>
#include <unordered_map>

class TextureManager {
 public:
  static TextureManager& get_instance() {
    static TextureManager instance;
    return instance;
  }

  // Called by background Taskflow threads to submit an image safely
  void submit_image(const std::string& pin_key, const cv::Mat& img);

  // Called ONCE per frame by the main ImGui thread to process submitted images
  // into OpenGL textures
  void process_pending();

  // Retrieve the OpenGL texture ID for a specific pin, returns 0 if none
  GLuint get_texture(const std::string& pin_key);

  // Clear all cached textures (e.g., when workflow restarts)
  void clear();

 private:
  TextureManager() = default;
  ~TextureManager() = default;

  TextureManager(const TextureManager&) = delete;
  TextureManager& operator=(const TextureManager&) = delete;

  std::mutex queue_mutex;
  std::unordered_map<std::string, cv::Mat> pending_mats;
  std::unordered_map<std::string, GLuint> active_textures;
};
