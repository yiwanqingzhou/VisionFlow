#pragma once
#include <string_view>

namespace TypeSystem {
// Primitive Types
inline constexpr std::string_view INT = "int";
inline constexpr std::string_view FLOAT = "float";
inline constexpr std::string_view DOUBLE = "double";
inline constexpr std::string_view BOOLEAN = "bool";
inline constexpr std::string_view STRING = "string";

// Array Types
inline constexpr std::string_view INT_ARRAY = "int_array";
inline constexpr std::string_view FLOAT_ARRAY = "float_array";
inline constexpr std::string_view DOUBLE_ARRAY = "double_array";
inline constexpr std::string_view STRING_ARRAY = "string_array";

// Vision & Geometry Types
inline constexpr std::string_view IMAGE = "cv::Mat";
inline constexpr std::string_view RECT = "cv::Rect";
inline constexpr std::string_view POINTCLOUD = "pointcloud";

// Generic Fallback
inline constexpr std::string_view ANY = "any";

// Control Flow
inline constexpr std::string_view TRIGGER = "trigger";

}  // namespace TypeSystem
