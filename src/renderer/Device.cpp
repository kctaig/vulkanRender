#include "renderer/Device.h"

#include <iostream>

namespace vr {

bool Device::initialize() {
    std::cout << "[Device] initialize\n";
    ready_ = true;
    return ready_;
}

void Device::shutdown() {
    if (!ready_) {
        return;
    }
    std::cout << "[Device] shutdown\n";
    ready_ = false;
}

} // namespace vr
