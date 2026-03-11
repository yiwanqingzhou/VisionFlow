#include "ui/texture_manager.hpp"

#include <iostream>

void TextureManager::submit_image(const std::string& pin_key,
                                  const cv::Mat& img) {
  if (img.empty()) return;
  std::lock_guard<std::mutex> lock(queue_mutex);
  pending_mats[pin_key] = img.clone();  // Clone to ensure background thread can
                                        // safely destroy its copy
}

void TextureManager::process_pending() {
  std::lock_guard<std::mutex> lock(queue_mutex);

  for (auto it = pending_mats.begin(); it != pending_mats.end();) {
    const std::string& key = it->first;
    cv::Mat& img = it->second;

    // Convert OpenCV memory to standard OpenGL format
    cv::Mat render_img;
    GLenum format = GL_RGB;
    if (img.channels() == 1) {
      cv::cvtColor(img, render_img, cv::COLOR_GRAY2RGB);
    } else if (img.channels() == 3) {
      cv::cvtColor(img, render_img, cv::COLOR_BGR2RGB);
    } else if (img.channels() == 4) {
      cv::cvtColor(img, render_img, cv::COLOR_BGRA2RGBA);
      format = GL_RGBA;
    } else {
      it = pending_mats.erase(it);
      continue;
    }

    // Allocate or update texture
    GLuint texture_id;
    if (active_textures.count(key)) {
      texture_id = active_textures[key];
      glBindTexture(GL_TEXTURE_2D, texture_id);
    } else {
      glGenTextures(1, &texture_id);
      active_textures[key] = texture_id;
      glBindTexture(GL_TEXTURE_2D, texture_id);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format, render_img.cols, render_img.rows, 0,
                 format, GL_UNSIGNED_BYTE, render_img.ptr());

    // Remove from pending once uploaded to GPU
    it = pending_mats.erase(it);
  }
}

GLuint TextureManager::get_texture(const std::string& pin_key) {
  if (active_textures.count(pin_key)) {
    return active_textures[pin_key];
  }
  return 0;  // 0 is invalid texture ID
}

void TextureManager::clear() {
  std::lock_guard<std::mutex> lock(queue_mutex);
  for (const auto& pair : active_textures) {
    glDeleteTextures(1, &pair.second);
  }
  active_textures.clear();
  pending_mats.clear();
}
