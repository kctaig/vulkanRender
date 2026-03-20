#include "renderer/RenderGraph.h"

#include <iostream>
#include <utility>

namespace vr {

RenderGraph::PassBuilder RenderGraph::addPass(std::string name, std::function<void()> execute) {
    passes_.push_back(PassNode{.name = std::move(name), .execute = std::move(execute)});
    return PassBuilder(passes_.back());
}

RenderGraph::PassBuilder& RenderGraph::PassBuilder::reads(std::string resource) {
    node_.reads.push_back(std::move(resource));
    return *this;
}

RenderGraph::PassBuilder& RenderGraph::PassBuilder::writes(std::string resource) {
    node_.writes.push_back(std::move(resource));
    return *this;
}

void RenderGraph::compile() {
    std::cout << "[RenderGraph] compile " << passes_.size() << " passes\n";
    for (const auto& pass : passes_) {
        std::cout << "  - " << pass.name << " | reads=" << pass.reads.size()
                  << " writes=" << pass.writes.size() << "\n";
    }
}

void RenderGraph::execute() const {
    for (const auto& pass : passes_) {
        if (pass.execute) {
            pass.execute();
        }
    }
}

void RenderGraph::clear() {
    passes_.clear();
}

} // namespace vr
