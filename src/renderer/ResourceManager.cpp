#include "renderer/ResourceManager.h"

namespace vr {

void ResourceManager::registerResource(const std::string& name) {
    resources_.insert(name);
}

bool ResourceManager::hasResource(const std::string& name) const {
    return resources_.contains(name);
}

void ResourceManager::clear() {
    resources_.clear();
}

} // namespace vr
