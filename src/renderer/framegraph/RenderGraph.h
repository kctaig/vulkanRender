/**
 * @file RenderGraph.h
 * @brief Declarative frame-graph types and execution interface.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace vr {

/**
 * @brief Frame-graph container that owns pass declarations and execution order.
 *
 * The graph supports both lambda-based and object-oriented pass registration.
 * During compilation, resource read/write dependencies are analyzed to produce
 * a deterministic pass order.
 */
class RenderGraph {
public:
    /** @brief Abstract pass contract used by object-oriented frame-graph nodes. */
    class IPass;

    /** @brief Internal pass node with dependency metadata and execute callback. */
    struct PassNode {
        std::string name;
        std::vector<std::string> reads;
        std::vector<std::string> writes;
        std::function<void()> execute;
    };

    /** @brief Builder used by passes to declare resource dependencies. */
    class PassBuilder {
    public:
        explicit PassBuilder(PassNode& node) : node_(node) {}

        /** @brief Declares that the current pass reads the named logical resource. */
        PassBuilder& reads(std::string resource);
        /** @brief Declares that the current pass writes the named logical resource. */
        PassBuilder& writes(std::string resource);

    private:
        PassNode& node_;
    };

    /** @brief Interface implemented by object-oriented frame-graph pass classes. */
    class IPass {
    public:
        virtual ~IPass() = default;
        /** @brief Returns pass name for logging, debugging, and profiling labels. */
        [[nodiscard]] virtual std::string_view name() const = 0;
        /** @brief Declares pass dependencies through reads/writes metadata. */
        virtual void setup(PassBuilder& builder) = 0;
        /** @brief Executes the pass body after graph ordering is resolved. */
        virtual void execute() = 0;
    };

    /** @brief Registers a lambda-backed pass and returns its dependency builder. */
    PassBuilder addPass(std::string name, std::function<void()> execute = {});
    /** @brief Registers an object-oriented pass instance. */
    void addPass(std::unique_ptr<IPass> pass);
    /** @brief Compiles dependencies and builds execution order. */
    bool compile();
    /** @brief Executes passes according to the compiled order. */
    void execute() const;
    /** @brief Clears all registered passes and cached compile data. */
    void clear();
    /** @brief Returns pass indices in the order selected by compile(). */
    [[nodiscard]] const std::vector<std::uint32_t>& executionOrder() const;

private:
    void rebuildPassNodesFromObjects();

    std::vector<std::unique_ptr<IPass>> passObjects_;
    std::vector<PassNode> passes_;
    std::vector<std::uint32_t> executionOrder_;
    bool compiled_ = false;
};

} // namespace vr

