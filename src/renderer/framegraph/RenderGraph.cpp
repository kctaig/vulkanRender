/**
 * @file RenderGraph.cpp
 * @brief Implementation for the RenderGraph module.
 */
#include "renderer/framegraph/RenderGraph.h"

#include <algorithm>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace vr {

RenderGraph::PassBuilder RenderGraph::addPass(std::string name, std::function<void()> execute) {
    passes_.push_back(PassNode{.name = std::move(name), .execute = std::move(execute)});
    compiled_ = false;
    return PassBuilder(passes_.back());
}

void RenderGraph::addPass(std::unique_ptr<IPass> pass) {
    if (pass != nullptr) {
        passObjects_.push_back(std::move(pass));
        compiled_ = false;
    }
}

RenderGraph::PassBuilder& RenderGraph::PassBuilder::reads(std::string resource) {
    node_.reads.push_back(std::move(resource));
    return *this;
}

RenderGraph::PassBuilder& RenderGraph::PassBuilder::writes(std::string resource) {
    node_.writes.push_back(std::move(resource));
    return *this;
}

void RenderGraph::rebuildPassNodesFromObjects() {
    if (passObjects_.empty()) {
        return;
    }

    passes_.clear();
    passes_.reserve(passObjects_.size());
    for (auto& passObject : passObjects_) {
        PassNode node{};
        node.name = std::string(passObject->name());
        node.execute = [&passObject]() {
            passObject->execute();
        };
        passes_.push_back(std::move(node));
        PassBuilder builder(passes_.back());
        passObject->setup(builder);
    }
}

bool RenderGraph::compile() {
    rebuildPassNodesFromObjects();

    executionOrder_.clear();
    if (passes_.empty()) {
        compiled_ = true;
        return true;
    }

    const std::size_t passCount = passes_.size();
    std::vector<std::unordered_set<std::uint32_t>> adjacency(passCount);
    std::vector<std::uint32_t> indegree(passCount, 0);
    std::unordered_map<std::string, std::uint32_t> lastWriter;

    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(passCount); ++i) {
        auto addEdge = [&](std::uint32_t from, std::uint32_t to) {
            if (from == to) {
                return;
            }
            if (adjacency[from].insert(to).second) {
                indegree[to] += 1;
            }
        };

        for (const std::string& readResource : passes_[i].reads) {
            auto foundWriter = lastWriter.find(readResource);
            if (foundWriter != lastWriter.end()) {
                addEdge(foundWriter->second, i);
            }
        }

        for (const std::string& writeResource : passes_[i].writes) {
            auto foundWriter = lastWriter.find(writeResource);
            if (foundWriter != lastWriter.end()) {
                addEdge(foundWriter->second, i);
            }
            lastWriter[writeResource] = i;
        }
    }

    std::queue<std::uint32_t> ready;
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(passCount); ++i) {
        if (indegree[i] == 0) {
            ready.push(i);
        }
    }

    while (!ready.empty()) {
        std::uint32_t current = ready.front();
        ready.pop();
        executionOrder_.push_back(current);

        for (std::uint32_t next : adjacency[current]) {
            indegree[next] -= 1;
            if (indegree[next] == 0) {
                ready.push(next);
            }
        }
    }

    const bool acyclic = executionOrder_.size() == passCount;
    if (!acyclic) {
        executionOrder_.clear();
        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(passCount); ++i) {
            executionOrder_.push_back(i);
        }
        std::cerr << "[RenderGraph] cycle detected, fallback to declaration order\n";
    }

    compiled_ = true;
    return acyclic;
}

void RenderGraph::execute() const {
    if (compiled_ && !executionOrder_.empty()) {
        for (std::uint32_t index : executionOrder_) {
            const auto& pass = passes_[index];
            if (pass.execute) {
                pass.execute();
            }
        }
        return;
    }

    for (const auto& pass : passes_) {
        if (pass.execute) {
            pass.execute();
        }
    }
}

void RenderGraph::clear() {
    passObjects_.clear();
    passes_.clear();
    executionOrder_.clear();
    compiled_ = false;
}

const std::vector<std::uint32_t>& RenderGraph::executionOrder() const {
    return executionOrder_;
}

} // namespace vr


