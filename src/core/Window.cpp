#include "core/Window.h"

#include <iostream>

namespace vr {

bool Window::create(const Desc& desc) {
    instance_ = GetModuleHandle(nullptr);

    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Window::wndProc;
    wc.hInstance = instance_;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "VulkanRendererWindowClass";

    if (RegisterClassEx(&wc) == 0) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            std::cerr << "[Window] RegisterClassEx failed: " << err << "\n";
            return false;
        }
    }

    RECT rect{0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    windowHandle_ = CreateWindowExA(
        0, wc.lpszClassName, desc.title.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
        rect.bottom - rect.top, nullptr, nullptr, instance_, this);

    if (windowHandle_ == nullptr) {
        std::cerr << "[Window] CreateWindowExA failed: " << GetLastError() << "\n";
        return false;
    }

    onMessage_ = desc.onMessage;
    ShowWindow(windowHandle_, SW_SHOWDEFAULT);
    UpdateWindow(windowHandle_);
    return true;
}

void Window::close() {
    shouldClose_ = true;
    if (windowHandle_) {
        DestroyWindow(windowHandle_);
        windowHandle_ = nullptr;
    }
}

bool Window::pumpMessages() {
    MSG msg{};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            shouldClose_ = true;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return !shouldClose_;
}

LRESULT CALLBACK Window::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Window* w = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        w = static_cast<Window*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(w));
        w->windowHandle_ = hwnd;
    } else {
        w = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (w != nullptr && w->onMessage_) {
        return w->onMessage_(hwnd, msg, wp, lp);
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

}  // namespace vr
