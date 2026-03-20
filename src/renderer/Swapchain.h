#pragma once

#include <cstdint>

#include "common/NonCopyable.h"

namespace vr {

class Swapchain : public NonCopyable {
public:
    bool create(std::uint32_t width, std::uint32_t height);
    void destroy();

    std::uint32_t width() const { return width_; }
    std::uint32_t height() const { return height_; }

private:
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    bool valid_ = false;
};

} // namespace vr
