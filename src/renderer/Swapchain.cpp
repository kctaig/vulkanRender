#include "renderer/Swapchain.h"

#include <iostream>

namespace vr {

bool Swapchain::create(std::uint32_t width, std::uint32_t height) {
    width_ = width;
    height_ = height;
    valid_ = true;
    std::cout << "[Swapchain] create " << width_ << "x" << height_ << "\n";
    return valid_;
}

void Swapchain::destroy() {
    if (!valid_) {
        return;
    }
    std::cout << "[Swapchain] destroy\n";
    valid_ = false;
}

} // namespace vr
