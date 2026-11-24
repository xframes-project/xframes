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
        ChangedPublicationPlan changed;
        try {
            if (m_surfaceQuarantined)
                throw CommitError("surface_quarantined", "Publication failed; recreate the native runtime");
            if (m_nativeSequence == std::numeric_limits<uint64_t>::max() || m_nativeRevision == std::numeric_limits<uint64_t>::max())
                throw CommitError("counter_overflow", "Native sequence/revision exhausted; create a new runtime");
            if (!m_frameScheduler.CanInvalidate())
                throw CommitError("counter_overflow", "Rendering ordering is unavailable; create a new runtime");
            if (batch.changedChildLists) {
                const AppliedSurface surface{m_hierarchy, m_parentOf, [this](int id) -> std::optional<ValidationNode> {
                    const auto it = m_elements.find(id);
                    if (it == m_elements.end()) return std::nullopt;
                    return ValidationNode{it->second->m_type, it->second->m_isRoot,
                        YGNodeHasMeasureFunc(it->second->m_layoutNode->m_node)};
                }};
                changed = ValidateChangedPublication(batch, surface, m_publicationOwnedIds, m_nativeRevision);
                result.destroyedIds.reserve(changed.destroyedIds.size());
            } else {
                ValidationTree tree;
                tree.children = m_hierarchy;
                for (const auto& [id, element] : m_elements)
                    tree.nodes.emplace(id, ValidationNode{element->m_type, element->m_isRoot,
                        YGNodeHasMeasureFunc(element->m_layoutNode->m_node)});
                plan = ValidatePublication(batch, std::move(tree), m_publicationOwnedIds, m_nativeRevision);
                // Reserve acknowledgment storage before touching resources/live state.
                result.destroyedIds.reserve(plan.destroyedIds.size());
            }
        } catch (const CommitError& error) {
            result.error = error;
        }
        validatedAt = diagnostics ? DiagnosticsNowMs() : 0;
        if (!result.error) {
            result.nativeSequence = ++m_nativeSequence;
            std::optional<size_t> operationIndex;
            try {
                // With eventsApplied, controlled props are stored, not patched; the sync below
                // decides whether each element shows them.
                std::unordered_set<int> controlled;
                const auto takeControlled = [&](int id, json& props) {
                    if (!batch.eventsApplied) return;
                    auto& element = *m_elements.at(id);
                    for (const auto& name : element.ControlledProps()) {
                        if (!props.contains(name)) continue;
                        element.m_committedControlled[name] = props[name];
                        props.erase(name);
                        controlled.insert(id);
                    }
                };
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
                            takeControlled(op.id, def);
                        } else {
                            takeControlled(op.id, def);
                            PatchElementUnlocked(def);
                        }
                    }
                }
                operationIndex.reset();
                if (batch.changedChildLists) ApplyChangedChildLists(changed, result.destroyedIds);
                else ApplyCompleteChildLists(plan, result.destroyedIds);
                if (batch.eventsApplied) SyncControlledUnlocked(*batch.eventsApplied, controlled);
                // Revision and generation become visible with exactly this tree.
                // Capture never reads a later publication's generation after drawing.
                if (!m_frameScheduler.Invalidate(FrameReason::Publication))
                    throw std::overflow_error("Invalidation generation exhausted during publication");
                result.nativeRevision = ++m_nativeRevision;
                m_visibleRevision.store(m_nativeRevision, std::memory_order_release);
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

// Only parents whose child list changed are re-linked. Re-linking an unchanged
// parent marks its Yoga subtree dirty and makes the next frame lay it out again.
// A child that moves leaves a parent whose list changes too, or one that is
// destroyed; both are disconnected before any candidate child is attached. Moving
// out of a removed ancestor retains the exact Element, Yoga node, internal subject
// and populated data.
void XFrames::ApplyCompleteChildLists(PublicationPlan& plan, std::vector<int>& destroyedIds) {
    std::unordered_set<int> relink;
    for (const auto& [parent, children] : plan.children) {
        if (parent == 0) continue;
        const auto previous = m_hierarchy.find(parent);
        if (!m_publicationOwnedIds.contains(parent) || previous == m_hierarchy.end() || previous->second != children)
            relink.insert(parent);
    }
    for (int id : m_publicationOwnedIds)
        if (relink.contains(id) || !plan.ownedIds.contains(id))
            YGNodeRemoveAllChildren(m_elements.at(id)->m_layoutNode->m_node);
    for (const auto& [parent, children] : plan.children) {
        m_hierarchy[parent] = children;
        if (parent == 0 || !relink.contains(parent)) continue;
        auto* element = m_elements.at(parent).get();
        for (size_t index = 0; index < children.size(); ++index)
            element->m_layoutNode->InsertChild(m_elements.at(children[index])->m_layoutNode.get(), index);
        element->m_maxBottomDirty = true;
    }
    // The plan is old-tree postorder filtered by final reachability;
    // destruction never recursively follows the newly published tree.
    for (int id : plan.destroyedIds) DestroyElementUnlocked(id, &destroyedIds);
    m_publicationOwnedIds.swap(plan.ownedIds);
    m_parentOf.clear();
    for (const auto& [parent, children] : plan.children)
        for (int child : children) m_parentOf[child] = parent;
}

// The same re-linking rule over the assigned lists only. Every other list, and its
// Yoga links, stays as it is: no work is done for unchanged parts of the tree.
void XFrames::ApplyChangedChildLists(const ChangedPublicationPlan& plan, std::vector<int>& destroyedIds) {
    std::unordered_set<int> relink;
    for (const auto& [parent, children] : plan.assigned) {
        if (parent == 0) continue;
        const auto previous = m_hierarchy.find(parent);
        if (!m_publicationOwnedIds.contains(parent) || previous == m_hierarchy.end() || previous->second != children)
            relink.insert(parent);
    }
    for (int id : relink) YGNodeRemoveAllChildren(m_elements.at(id)->m_layoutNode->m_node);
    for (int id : plan.destroyedIds) YGNodeRemoveAllChildren(m_elements.at(id)->m_layoutNode->m_node);
    // Parent links: drop the ones the assigned lists held, then record the new ones.
    for (const auto& [parent, children] : plan.assigned) {
        const auto previous = m_hierarchy.find(parent);
        if (previous == m_hierarchy.end()) continue;
        for (int child : previous->second)
            if (const auto link = m_parentOf.find(child); link != m_parentOf.end() && link->second == parent)
                m_parentOf.erase(link);
    }
    for (const auto& [parent, children] : plan.assigned) {
        for (int child : children) m_parentOf[child] = parent;
        m_hierarchy[parent] = children;
        if (parent == 0 || !relink.contains(parent)) continue;
        auto* element = m_elements.at(parent).get();
        for (size_t index = 0; index < children.size(); ++index)
            element->m_layoutNode->InsertChild(m_elements.at(children[index])->m_layoutNode.get(), index);
        element->m_maxBottomDirty = true;
    }
    for (int id : plan.created) m_publicationOwnedIds.insert(id);
    for (int id : plan.destroyedIds) {
        m_parentOf.erase(id);
        m_publicationOwnedIds.erase(id);
        DestroyElementUnlocked(id, &destroyedIds);
    }
}

// The controlled-values rule. An element edited after eventsApplied keeps what the user
// typed: the application has not seen that edit, and its event will reach it. Any other
// element that was patched, created or edited shows its committed controlled values, even
// when this commit does not carry them, so a rejected or transformed edit is undone.
void XFrames::SyncControlledUnlocked(uint64_t eventsApplied, const std::unordered_set<int>& touched) {
    const std::lock_guard<std::mutex> lock(m_eventMutex);
    std::unordered_set<int> candidates(touched);
    for (const auto& [id, sequence] : m_unsyncedEdits) candidates.insert(id);
    for (int id : candidates) {
        if (const auto edit = m_unsyncedEdits.find(id); edit != m_unsyncedEdits.end()) {
            if (edit->second > eventsApplied) continue;
            m_unsyncedEdits.erase(edit);
        }
        const auto element = m_elements.find(id);
        if (element == m_elements.end() || element->second->ControlledProps().empty()) continue;
        element->second->ApplyControlled(element->second->m_committedControlled);
    }
}

uint64_t XFrames::NoteWidgetEvent(int id) {
    const std::lock_guard<std::mutex> lock(m_eventMutex);
    const auto sequence = ++m_eventSequence;
    if (id != 0) m_unsyncedEdits[id] = sequence;
    return sequence;
}

uint64_t XFrames::LastEventSequence() {
    const std::lock_guard<std::mutex> lock(m_eventMutex);
    return m_eventSequence;
}

json XFrames::GetCommitState() {
    const std::lock_guard<std::mutex> lock(m_commitMutex);
    json state = {{"schemaVersion", 2}, {"surfaceId", 0}, {"initialized", !m_runtimeDisposed}, {"nativeSequence", std::to_string(m_nativeSequence)},
        {"nativeRevision", std::to_string(m_nativeRevision)},
        {"surfaceStatus", m_runtimeDisposed ? "disposed" : m_surfaceQuarantined ? "quarantined" : "healthy"}, {"managedCount", m_publicationOwnedIds.size()},
        {"commitFeatures", json::array({"changedChildLists", "controlledValues"})}};
    if (m_diagnosticsEnabled.load(std::memory_order_relaxed)) state["lastTransaction"] = m_lastCommitDiagnostics;
    return state;
}
