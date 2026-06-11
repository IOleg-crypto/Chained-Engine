#include "windows_window.h"
#include "engine/core/log.h"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

namespace CHEngine
{

WindowsWindow::WindowsWindow(const WindowProperties& properties)
{
    Init(properties);
}

WindowsWindow::~WindowsWindow()
{
    Shutdown();
}

void WindowsWindow::Init(const WindowProperties& properties)
{
    m_Width = properties.Width;
    m_Height = properties.Height;
    m_Title = properties.Title;
    m_VSync = properties.VSync;

    CH_CORE_INFO("Initializing Windows Window: {} ({}x{})", m_Title, m_Width, m_Height);

    unsigned int flags = FLAG_MSAA_4X_HINT;
    if (properties.Resizable)
    {
        flags |= FLAG_WINDOW_RESIZABLE;
    }
    if (properties.Fullscreen)
    {
        flags |= FLAG_FULLSCREEN_MODE;
    }

    SetConfigFlags(flags);
    InitWindow(m_Width, m_Height, m_Title.c_str());

    if (m_VSync)
    {
        SetTargetFramesPerSecond(GetMonitorRefreshRate(GetCurrentMonitor()));
    }
    else
    {
        SetTargetFramesPerSecond(properties.TargetFramesPerSecond);
    }

    m_WindowHandle = glfwGetCurrentContext();
    SetExitKey(KEY_NULL);
}

void WindowsWindow::Shutdown()
{
    CloseWindow();
    CH_CORE_INFO("Windows Window Closed");
}

void WindowsWindow::BeginFrame()
{
    BeginDrawing();
    ClearBackground(DARKGRAY);
}

void WindowsWindow::EndFrame()
{
    EndDrawing();
}

bool WindowsWindow::ShouldClose() const
{
    return WindowShouldClose();
}

void WindowsWindow::SetTitle(const std::string& title)
{
    m_Title = title;
    SetWindowTitle(m_Title.c_str());
}

void WindowsWindow::SetSize(int width, int height)
{
    m_Width = width;
    m_Height = height;
    SetWindowSize(m_Width, m_Height);
}

void WindowsWindow::SetSizeDirect(int width, int height)
{
    m_Width = width;
    m_Height = height;
}

void WindowsWindow::ToggleFullscreen()
{
    ::ToggleFullscreen();
}

void WindowsWindow::SetFullscreen(bool enabled)
{
    if (IsWindowFullscreen() != enabled)
    {
        ::ToggleFullscreen();
    }
}

void WindowsWindow::SetVSync(bool enabled)
{
    m_VSync = enabled;
    if (m_VSync)
    {
        SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));
    }
}

void WindowsWindow::SetAntialiasing(bool enabled)
{
    if (enabled)
    {
        SetWindowState(FLAG_MSAA_4X_HINT);
    }
    else
    {
        ClearWindowState(FLAG_MSAA_4X_HINT);
    }
}

void WindowsWindow::SetTargetFramesPerSecond(int framesPerSecond)
{
    if (!m_VSync)
    {
        SetTargetFPS(framesPerSecond);
    }
}

void WindowsWindow::SetWindowIcon(Image icon)
{
    ::SetWindowIcon(icon);
}

} // namespace CHEngine
