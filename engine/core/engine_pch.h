#ifndef ENGINE_PCH_H
#define ENGINE_PCH_H

#include <assert.h>
#include <cassert>
#ifndef assert
  #define assert(expr) ((void)0) 
#endif
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <functional>

#ifdef CH_PLATFORM_WINDOWS
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  
  #define ShowCursor _win_ShowCursor
  #define CloseWindow _win_CloseWindow
  #define Rectangle _win_Rectangle
  #define DrawText _win_DrawText
  #define DrawTextEx _win_DrawTextEx
  #define LoadImage _win_LoadImage
  
  #include <windows.h>
  
  #undef ShowCursor
  #undef CloseWindow
  #undef Rectangle
  #undef DrawText
  #undef DrawTextEx
  #undef LoadImage
#endif

#include <raylib.h>
#include <entt/entt.hpp>
#include <yaml-cpp/yaml.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#endif