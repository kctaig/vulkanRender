#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace vr {

class RenderGraph {
public:
    struct PassNode {
        std::string name;
        std::vector<std::string> reads;
        std::vector<std::string> writes;
        std::function<void()> execute;
    };

    class PassBuilder {
    public:
        explicit PassBuilder(PassNode& node) : node_(node) {}

        PassBuilder& reads(std::string resource);
        PassBuilder& writes(std::string resource);

    private:
        PassNode& node_;
    };

    PassBuilder addPass(std::string name, std::function<void()> execute = {});
    void compile();
    void execute() const;
    void clear();

private:
    std::vector<PassNode> passes_;
};

} // namespace vr
