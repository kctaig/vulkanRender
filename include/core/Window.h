#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdint>
#include <functional>
#include <string>

namespace vr {

/// Thin Win32 window wrapper — no Vulkan dependency.
class Window {
  public:
    using MessageCallback = std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)>;

    struct Desc {
        std::string title = "Vulkan Renderer";
        std::uint32_t width = 1600;
        std::uint32_t height = 900;
        MessageCallback onMessage; // external handler (e.g. Renderer input)
    };

    bool create(const Desc& desc);
    void close();
    bool pumpMessages();
    void* nativeHandle() const { return windowHandle_; }

  private:
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);

    HWND windowHandle_ = nullptr;
    HINSTANCE instance_ = nullptr;
    MessageCallback onMessage_;
    bool shouldClose_ = false;
};

}  // namespace vr
