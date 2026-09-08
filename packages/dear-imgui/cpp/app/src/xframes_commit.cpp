#include "xframes.h"
#include "element/element.h"
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
        if (m_runtimeDisposed) throw CommitError("runtime_disposed", "Native runtime has been disposed");
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

CommitResult XFrames::DispatchCommit(CommitBatch batch) {
    auto request = std::make_shared<CommitRequest>();
    request->batch = std::move(batch);
    m_elementOpSubject.get_observer().on_next(std::weak_ptr<CommitRequest>(request));
    // Also notify terminal failures whose error receipt could not be allocated.
    // Tree visibility locks have been released by the subject handler.
    if (request->exception || request->result.status != "rejected") m_frameScheduler.Notify();
    if (request->exception) std::rethrow_exception(request->exception);
    if (!request->completed) throw std::logic_error("Structural subject did not complete synchronously");
    return std::move(request->result);
}

CommitResult XFrames::ApplyCommitOperations(CommitBatch& batch) {
    CommitResult result;
    result.nativeRevision = m_nativeRevision;
    result.correlationId = batch.correlationId;
    const bool diagnostics = m_diagnosticsEnabled.load(std::memory_order_relaxed);
    const auto start = diagnostics ? DiagnosticsNowMs() : 0;
    double lockedAt = start, validatedAt = start, appliedAt = start;
    {
        // Dispatch -> serialized subject -> hierarchy -> elements. All public
        // structural writers use dispatch; render/getters participate in these
        // same tree locks. No operation drops them or calls back into JavaScript.
        const std::lock_guard<std::mutex> hierarchyLock(m_hierarchy_mutex);
        const std::lock_guard<std::mutex> elementsLock(m_elements_mutex);
        lockedAt = diagnostics ? DiagnosticsNowMs() : 0;
        PublicationPlan plan;
        try {
            if (m_surfaceQuarantined)
                throw CommitError("surface_quarantined", "Publication failed; recreate the native runtime");
            if (m_nativeSequence == std::numeric_limits<uint64_t>::max() || m_nativeRevision == std::numeric_limits<uint64_t>::max())
                throw CommitError("counter_overflow", "Native sequence/revision exhausted; create a new runtime");
            if (!m_frameScheduler.CanInvalidate())
                throw CommitError("counter_overflow", "Rendering ordering is unavailable; create a new runtime");
            ValidationTree tree;
            tree.children = m_hierarchy;
            for (const auto& [id, element] : m_elements)
                tree.nodes.emplace(id, ValidationNode{element->m_type, element->m_isRoot,
                    YGNodeHasMeasureFunc(element->m_layoutNode->m_node)});
            plan = ValidatePublication(batch, std::move(tree), m_publicationOwnedIds, m_nativeRevision);
            // Reserve acknowledgment storage before touching resources/live state.
            result.destroyedIds.reserve(plan.destroyedIds.size());
        } catch (const CommitError& error) {
            result.error = error;
        }
        validatedAt = diagnostics ? DiagnosticsNowMs() : 0;
        if (!result.error) {
            result.nativeSequence = ++m_nativeSequence;
            std::optional<size_t> operationIndex;
            try {
                // All creates precede patches, so forward references in the
                // complete candidate have one unambiguous meaning.
                for (auto kind : {CommitOp::Create, CommitOp::Patch}) {
                    for (size_t index = 0; index < batch.operations.size(); ++index) {
                        const auto& op = batch.operations[index];
                        if (op.op != kind) continue;
                        operationIndex = index;
                        auto def = op.props;
                        def["id"] = op.id;
                        if (kind == CommitOp::Create) {
                            def["type"] = op.elementType;
                            CreateElementUnlocked(def);
                        } else PatchElementUnlocked(def);
                    }
                }
                operationIndex.reset();
                // Disconnect every previous Yoga owner before attaching any
                // candidate child. Moving out of a removed ancestor retains the
                // exact Element, Yoga node, internal subject and populated data.
                for (int id : m_publicationOwnedIds)
                    YGNodeRemoveAllChildren(m_elements.at(id)->m_layoutNode->m_node);
                for (const auto& [parent, children] : plan.children) {
                    m_hierarchy[parent] = children;
                    if (parent == 0) continue;
                    auto* element = m_elements.at(parent).get();
                    for (size_t index = 0; index < children.size(); ++index)
                        element->m_layoutNode->InsertChild(m_elements.at(children[index])->m_layoutNode.get(), index);
                    element->m_maxBottomDirty = true;
                }
                // The plan is old-tree postorder filtered by final reachability;
                // destruction never recursively follows the newly published tree.
                for (int id : plan.destroyedIds) DestroyElementUnlocked(id, &result.destroyedIds);
                m_publicationOwnedIds.swap(plan.ownedIds);
                // Revision and generation become visible with exactly this tree.
                // Capture never reads a later publication's generation after drawing.
                if (!m_frameScheduler.Invalidate(FrameReason::Publication))
                    throw std::overflow_error("Invalidation generation exhausted during publication");
                result.nativeRevision = ++m_nativeRevision;
                result.status = "applied";
            } catch (const std::exception& error) {
                // Quarantine is established under the visibility locks even if
                // constructing an error acknowledgment itself cannot allocate.
                m_surfaceQuarantined = true;
                m_frameScheduler.Quarantine();
                result.status = "failed";
                result.error.emplace("application_error", std::string(error.what()).substr(0, 512), operationIndex);
            } catch (...) {
                m_surfaceQuarantined = true;
                m_frameScheduler.Quarantine();
                result.status = "failed";
                result.error.emplace("application_error", "Unexpected native publication failure", operationIndex);
            }
        }
        appliedAt = diagnostics ? DiagnosticsNowMs() : 0;
    }
    if (diagnostics) m_lastCommitDiagnostics = {
        {"nativeSequence", result.nativeSequence ? json(std::to_string(*result.nativeSequence)) : json(nullptr)},
        {"nativeRevision", std::to_string(m_nativeRevision)}, {"operationCount", batch.operations.size()},
        {"managedCount", m_publicationOwnedIds.size()}, {"visibilityLockWaitMs", lockedAt - start},
        {"visibilityLockHeldMs", appliedAt - lockedAt}, {"validationMs", validatedAt - lockedAt},
        {"applicationMs", appliedAt - validatedAt}, {"status", result.status},
        {"errorCode", result.error ? json(result.error->code) : json(nullptr)}};
    // Completion/result delivery follows the release of both visibility locks.
    return result;
}

json XFrames::GetCommitState() {
    const std::lock_guard<std::mutex> lock(m_commitMutex);
    json state = {{"schemaVersion", 2}, {"surfaceId", 0}, {"initialized", !m_runtimeDisposed}, {"nativeSequence", std::to_string(m_nativeSequence)},
        {"nativeRevision", std::to_string(m_nativeRevision)},
        {"surfaceStatus", m_runtimeDisposed ? "disposed" : m_surfaceQuarantined ? "quarantined" : "healthy"}, {"managedCount", m_publicationOwnedIds.size()}};
    if (m_diagnosticsEnabled.load(std::memory_order_relaxed)) state["lastTransaction"] = m_lastCommitDiagnostics;
    return state;
}
