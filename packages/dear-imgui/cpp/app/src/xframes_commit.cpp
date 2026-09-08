#include "xframes.h"
#include "element/element.h"
#include <GLFW/glfw3.h>
#include <limits>

using namespace xframes;

CommitResult XFrames::ApplyCommit(std::string_view serializedCommit) {
    const std::lock_guard<std::mutex> lock(m_commitMutex);
    const bool diagnostics = m_diagnosticsEnabled.load(std::memory_order_relaxed);
    const auto start = diagnostics ? DiagnosticsNowMs() : 0;
    if (diagnostics) m_lastCommitDiagnostics = json::object();
    CommitResult result;
    result.nativeRevision = m_nativeRevision;
    double parsedAt = start;
    try {
        auto batch = ParseCommit(json::parse(serializedCommit));
        parsedAt = diagnostics ? DiagnosticsNowMs() : 0;
        result = DispatchCommit(std::move(batch));
    } catch (const CommitError& error) {
        result.error = error;
    } catch (const json::parse_error&) {
        result.error.emplace("invalid_json", "Malformed JSON");
    } catch (const json::out_of_range&) {
        // JSON's numeric decoder can reject a number before an owned batch exists.
        result.error.emplace("invalid_json", "JSON number is out of range");
    }
    if (diagnostics) {
        if (!m_lastCommitDiagnostics.is_object()) m_lastCommitDiagnostics = json::object();
        m_lastCommitDiagnostics["wireBytes"] = serializedCommit.size();
        m_lastCommitDiagnostics["parseMs"] = parsedAt - start;
        m_lastCommitDiagnostics["totalMs"] = DiagnosticsNowMs() - start;
        m_lastCommitDiagnostics["status"] = result.status;
        m_lastCommitDiagnostics["errorCode"] = result.error ? json(result.error->code) : json(nullptr);
        m_lastCommitDiagnostics["operationIndex"] = result.error && result.error->operationIndex ? json(*result.error->operationIndex) : json(nullptr);
        m_lastCommitDiagnostics["nativeSequence"] = result.nativeSequence ? json(std::to_string(*result.nativeSequence)) : json(nullptr);
        m_lastCommitDiagnostics["nativeRevision"] = std::to_string(result.nativeRevision);
    }
    return result;
}

CommitResult XFrames::ApplyCompatibility(json operation) {
    const std::lock_guard<std::mutex> lock(m_commitMutex);
    const bool diagnostics = m_diagnosticsEnabled.load(std::memory_order_relaxed);
    const auto start = diagnostics ? DiagnosticsNowMs() : 0;
    auto batch = ParseCommit({{"schemaVersion", 1}, {"surfaceId", 0}, {"operations", json::array({std::move(operation)})}});
    batch.compatibility = true;
    const auto envelopeEnd = diagnostics ? DiagnosticsNowMs() : 0;
    auto result = DispatchCommit(std::move(batch));
    if (diagnostics) {
        m_lastCommitDiagnostics["envelopeMs"] = envelopeEnd - start;
        m_lastCommitDiagnostics["dispatchMs"] = DiagnosticsNowMs() - start;
    }
    if (result.error) throw *result.error;
    return result;
}

CommitResult XFrames::DispatchCommit(CommitBatch batch) {
    auto request = std::make_shared<CommitRequest>();
    request->batch = std::move(batch);
    m_elementOpSubject.get_observer().on_next(std::weak_ptr<CommitRequest>(request));
    if (request->exception) std::rethrow_exception(request->exception);
    if (!request->completed) throw std::logic_error("Structural subject did not complete synchronously");
#ifndef __EMSCRIPTEN__
    if (request->result.status != "rejected") glfwPostEmptyEvent();
#endif
    return std::move(request->result);
}

CommitResult XFrames::ApplyCommitOperations(CommitBatch& batch) {
    CommitResult result;
    result.nativeRevision = m_nativeRevision;
    result.correlationId = batch.correlationId;
    const bool diagnostics = m_diagnosticsEnabled.load(std::memory_order_relaxed);
    const auto start = diagnostics ? DiagnosticsNowMs() : 0;
    try {
        if (m_nativeSequence == std::numeric_limits<uint64_t>::max() || m_nativeRevision == std::numeric_limits<uint64_t>::max())
            throw CommitError("counter_overflow", "Native sequence/revision exhausted; create a new runtime");
        ValidationTree tree;
        {
            const std::lock_guard<std::mutex> hierarchyLock(m_hierarchy_mutex);
            const std::lock_guard<std::mutex> elementsLock(m_elements_mutex);
            tree.children = m_hierarchy;
            for (const auto& [id, element] : m_elements)
                tree.nodes.emplace(id, ValidationNode{element->m_type, element->m_isRoot,
                    YGNodeHasMeasureFunc(element->m_layoutNode->m_node)});
        }
        // The outer dispatch mutex stays held through validation and application.
        // No competing public structural call can invalidate this model.
        ValidateCommit(batch, std::move(tree));
    } catch (const CommitError& error) {
        result.error = error;
        if (diagnostics) m_lastCommitDiagnostics = {
            {"status", "rejected"}, {"nativeSequence", nullptr}, {"nativeRevision", std::to_string(m_nativeRevision)},
            {"operationCount", batch.operations.size()}, {"compatibility", batch.compatibility},
            {"validationMs", DiagnosticsNowMs() - start}, {"applicationMs", 0},
            {"errorCode", error.code}, {"operationIndex", error.operationIndex ? json(*error.operationIndex) : json(nullptr)}};
        return result;
    }
    const auto validatedAt = diagnostics ? DiagnosticsNowMs() : 0;
    result.nativeSequence = ++m_nativeSequence;
    size_t index = 0;
    try {
        for (const auto& op : batch.operations) {
            if (!op.skip) {
                switch (op.op) {
                case CommitOp::Create: {
                    auto def = op.props;
                    def["id"] = op.id; def["type"] = op.elementType;
                    CreateElement(def);
                    break;
                }
                case CommitOp::Patch: {
                    auto def = op.props;
                    def["id"] = op.id;
                    PatchElement(def);
                    break;
                }
                case CommitOp::SetChildren: {
                    auto destroyed = SetChildren({{"parentId", op.id}, {"childrenIds", op.children}});
                    result.destroyedIds.insert(result.destroyedIds.end(), destroyed.begin(), destroyed.end());
                    break;
                }
                case CommitOp::AppendChild:
                    AppendChild({{"parentId", op.id}, {"childId", op.children.front()}});
                    break;
                }
            }
            ++index;
        }
        result.nativeRevision = ++m_nativeRevision;
        result.status = "applied";
    } catch (const std::exception& error) {
        // Resource/allocation/constructor failures are not validation rejections.
        // A prefix may have applied: report failure and actual completed destruction,
        // consume the application sequence, and never advance the success revision.
        result.status = "failed";
        result.error.emplace("application_error", std::string(error.what()).substr(0, 512), index);
    }
    if (diagnostics) m_lastCommitDiagnostics = {
        {"nativeSequence", std::to_string(*result.nativeSequence)}, {"nativeRevision", std::to_string(m_nativeRevision)},
        {"operationCount", batch.operations.size()}, {"compatibility", batch.compatibility},
        {"validationMs", validatedAt - start}, {"applicationMs", DiagnosticsNowMs() - validatedAt},
        {"status", result.status}, {"errorCode", result.error ? json(result.error->code) : json(nullptr)}};
    // Each private helper has released its tree locks before completion is published.
    return result;
}

json XFrames::GetCommitState() {
    const std::lock_guard<std::mutex> lock(m_commitMutex);
    json state = {{"schemaVersion", 1}, {"surfaceId", 0}, {"initialized", true}, {"nativeSequence", std::to_string(m_nativeSequence)},
        {"nativeRevision", std::to_string(m_nativeRevision)}};
    if (m_diagnosticsEnabled.load(std::memory_order_relaxed)) state["lastTransaction"] = m_lastCommitDiagnostics;
    return state;
}
