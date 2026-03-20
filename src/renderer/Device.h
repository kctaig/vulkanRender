#pragma once

#include <string>

#include "common/NonCopyable.h"

namespace vr {

class Device : public NonCopyable {
public:
    bool initialize();
    void shutdown();

    const std::string& name() const { return name_; }

private:
    std::string name_ = "Vulkan Logical Device (stub)";
    bool ready_ = false;
};

} // namespace vr
