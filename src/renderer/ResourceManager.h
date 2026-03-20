#pragma once

#include <string>
#include <unordered_set>

namespace vr {

class ResourceManager {
public:
    void registerResource(const std::string& name);
    bool hasResource(const std::string& name) const;
    void clear();

private:
    std::unordered_set<std::string> resources_;
};

} // namespace vr
