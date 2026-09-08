#include "commit.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <charconv>
#include <unordered_set>

namespace xframes {
using json = nlohmann::json;
namespace {
void Require(bool condition, const char* code, const char* message) {
    if (!condition) throw CommitError(code, message);
}
void Fields(const json& value, std::initializer_list<const char*> required,
            std::initializer_list<const char*> optional = {}) {
    Require(value.is_object(), "invalid_field", "Expected an object");
    for (auto key : required) Require(value.contains(key), "missing_field", key);
    for (auto it = value.begin(); it != value.end(); ++it) {
        const auto matches = [&](const char* key) { return it.key() == key; };
        Require(std::any_of(required.begin(), required.end(), matches) ||
                std::any_of(optional.begin(), optional.end(), matches), "unknown_field", "Unknown envelope or operation field");
    }
}
}

json CommitResult::ToJson() const {
    json result = {{"schemaVersion", 2}, {"surfaceId", 0}, {"status", status},
        {"nativeSequence", nativeSequence ? json(std::to_string(*nativeSequence)) : json(nullptr)},
        {"nativeRevision", std::to_string(nativeRevision)}, {"destroyedIds", destroyedIds}};
    if (correlationId) result["correlationId"] = *correlationId;
    if (error) result["error"] = {{"code", error->code}, {"message", error->what()},
        {"operationIndex", error->operationIndex ? json(*error->operationIndex) : json(nullptr)}};
    return result;
}

CommitResult UninitializedCommitResult() {
    CommitResult result;
    result.error.emplace("runtime_not_ready", "Initialize the native runtime and wait for its existing ready callback");
    return result;
}

json UninitializedCommitState() {
    return {{"schemaVersion", 2}, {"surfaceId", 0}, {"initialized", false},
        {"nativeSequence", "0"}, {"nativeRevision", "0"}, {"surfaceStatus", "uninitialized"}, {"managedCount", 0}};
}

int ParseNativeId(const json& value, bool allowContainer) {
    Require(value.is_number_integer() && value >= (allowContainer ? 0 : 1) && value <= INT_MAX,
        "invalid_id", "Native IDs must be integers in [1, 2147483647]; only a parent may be container 0");
    return value.get<int>();
}

int ParseBindingId(double value, bool allowContainer) {
    Require(std::isfinite(value) && std::floor(value) == value && value >= (allowContainer ? 0 : 1) && value <= INT_MAX,
        "invalid_id", "Binding ID is outside the native integer range");
    return static_cast<int>(value);
}

CommitBatch ParseCommit(json wire) {
    Require(wire.is_object() && wire.contains("schemaVersion"), "missing_field", "schemaVersion");
    Require(wire["schemaVersion"].is_number_integer() && wire["schemaVersion"] == 2,
        "unsupported_version", "Only schemaVersion 2 is supported; the alpha v1 structural API was removed");
    Fields(wire, {"schemaVersion", "surfaceId", "baseRevision", "rootChildren", "operations"}, {"correlationId"});
    Require(wire["surfaceId"].is_number_integer() && wire["surfaceId"] == 0,
        "unsupported_surface", "Only surfaceId 0 is supported");
    Require(wire["operations"].is_array(), "invalid_field", "operations must be an array");
    CommitBatch batch;
    {
        Require(wire["baseRevision"].is_string(), "invalid_field", "baseRevision must be a canonical uint64 decimal string");
        const auto& value = wire["baseRevision"].get_ref<const std::string&>();
        Require(!value.empty() && (value == "0" || value.front() != '0') &&
            value.find_first_not_of("0123456789") == std::string::npos,
            "invalid_field", "baseRevision must be a canonical uint64 decimal string");
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), batch.baseRevision);
        Require(parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size(), "invalid_field", "baseRevision exceeds uint64");
        Require(wire["rootChildren"].is_array(), "invalid_field", "rootChildren must be an array");
        for (const auto& id : wire["rootChildren"]) batch.rootChildren.push_back(ParseNativeId(id));
    }
    if (wire.contains("correlationId")) {
        Require(wire["correlationId"].is_string() && wire["correlationId"].get_ref<const std::string&>().size() <= 128,
            "invalid_field", "correlationId must be a string of at most 128 UTF-8 bytes");
        batch.correlationId = wire["correlationId"].get<std::string>();
    }
    for (auto& def : wire["operations"]) {
        try {
            Require(def.is_object() && def.contains("op"), "missing_field", "Operation requires op");
            Require(def["op"].is_string(), "invalid_field", "op must be a string");
            auto name = def["op"].get<std::string>();
            CommitOperation op;
            if (name == "create" || name == "patch") {
                if (name == "create") Fields(def, {"op", "id", "elementType", "props"});
                else Fields(def, {"op", "id", "props"});
                op.op = name == "create" ? CommitOp::Create : CommitOp::Patch;
                op.id = ParseNativeId(def["id"]);
                Require(def["props"].is_object(), "invalid_props", "props must be an object");
                Require(!def["props"].contains("id") && !def["props"].contains("type"),
                    "immutable_identity", "props cannot contain native id or type; public IDs belong to JavaScript");
                op.props = std::move(def["props"]);
                if (op.op == CommitOp::Create) {
                    Require(def["elementType"].is_string(), "invalid_field", "elementType must be a string");
                    op.elementType = def["elementType"].get<std::string>();
                }
            } else if (name == "setChildren") {
                op.op = CommitOp::SetChildren;
                Fields(def, {"op", "parentId", "childrenIds"});
                Require(def["childrenIds"].is_array(), "invalid_field", "childrenIds must be an array");
                for (auto& id : def["childrenIds"]) op.children.push_back(ParseNativeId(id));
                op.id = ParseNativeId(def["parentId"]);
            } else throw CommitError("unsupported_operation", "Unsupported operation");
            batch.operations.push_back(std::move(op));
        } catch (CommitError& error) {
            error.operationIndex = batch.operations.size();
            throw;
        }
    }
    return batch;
}

bool IsCommitElementType(const std::string& type) {
    static const std::unordered_set<std::string> types = {
        "node", "group", "child", "di-window", "separator", "collapsing-header", "tab-bar", "tab-item", "tree-node",
        "di-table", "clipped-multi-line-text-renderer", "di-image", "map-view", "di-js-canvas", "di-lua-canvas",
        "di-janet-canvas", "plot-bar", "plot-heatmap", "plot-histogram", "plot-line", "plot-pie-chart", "plot-scatter",
        "plot-candlestick", "item-tooltip", "color-indicator", "combo", "slider", "input-text", "multi-slider",
        "checkbox", "color-picker", "di-button", "progress-bar", "separator-text", "bullet-text", "unformatted-text",
        "disabled-text", "text-wrap"};
    return types.contains(type);
}

bool IsMeasuredElementType(const std::string& type) {
    static const std::unordered_set<std::string> types = {"di-button", "checkbox", "color-picker",
        "combo", "input-text", "multi-slider", "slider", "separator-text", "bullet-text", "unformatted-text",
        "disabled-text", "di-table", "di-image", "clipped-multi-line-text-renderer", "separator", "progress-bar"};
    return types.contains(type);
}

PublicationPlan ValidatePublication(CommitBatch& batch, ValidationTree tree,
    const std::unordered_set<int>& managedIds, uint64_t nativeRevision) {
    Require(batch.baseRevision == nativeRevision, "stale_revision", "baseRevision does not match the current structural revision");
    // A publication may claim freshly created objects only. Unrelated standalone
    // objects are neither adopted nor swept, including pre-existing virtual roots.
    for (int root : tree.children[0])
        Require(managedIds.contains(root), "ownership_conflict", "Container has an unowned root");
    std::unordered_set<int> created;
    for (size_t index = 0; index < batch.operations.size(); ++index) {
        const auto& op = batch.operations[index];
        if (op.op != CommitOp::Create) continue;
        try {
            Require(!tree.nodes.contains(op.id), "duplicate_id", "Create target already exists; a publication cannot recreate an old lifetime");
            Require(IsCommitElementType(op.elementType), "invalid_element_type", "Unsupported elementType");
            ValidateCommitProps(op.elementType, op.props, true);
            tree.nodes.emplace(op.id, ValidationNode{op.elementType, op.props.value("root", false), IsMeasuredElementType(op.elementType)});
            created.insert(op.id);
        } catch (CommitError& error) { error.operationIndex = index; throw; }
    }
    PublicationPlan plan;
    plan.children.emplace(0, batch.rootChildren);
    std::unordered_map<int, size_t> assignments;
    std::vector<int> parents{0};
    for (size_t index = 0; index < batch.operations.size(); ++index) {
        auto& op = batch.operations[index];
        if (op.op == CommitOp::Create) continue;
        try {
            Require(tree.nodes.contains(op.id), "missing_target", "Publication target does not exist");
            Require(managedIds.contains(op.id) || created.contains(op.id), "ownership_conflict", "Publication target is not owned by this surface");
            if (op.op == CommitOp::Patch) {
                op.elementType = tree.nodes.at(op.id).type;
                ValidateCommitProps(op.elementType, op.props, false);
            } else {
                Require(op.op == CommitOp::SetChildren, "unsupported_operation", "Publication requires complete child lists");
                Require(assignments.emplace(op.id, index).second, "duplicate_assignment", "A publication assigns each parent's children exactly once");
                plan.children.emplace(op.id, op.children);
                parents.push_back(op.id);
            }
        } catch (CommitError& error) { error.operationIndex = index; throw; }
    }
    // Validate all declarations, including disconnected ones, before reachability.
    // References to any create in the envelope are permitted regardless of order.
    std::unordered_map<int, int> owners;
    for (int parent : parents) {
        const auto& children = plan.children.at(parent);
        try {
            Require(parent == 0 || children.empty() || !tree.nodes.at(parent).measured,
                "invalid_relationship", "A measured Yoga leaf cannot own children");
            std::unordered_set<int> unique;
            for (int child : children) {
                Require(unique.insert(child).second, "duplicate_child", "A child list cannot contain duplicate IDs");
                Require(child != parent, "cycle", "A node cannot contain itself");
                Require(tree.nodes.contains(child), "missing_target", "Child does not exist in the complete candidate");
                Require(managedIds.contains(child) || created.contains(child), "ownership_conflict", "Child is not owned by this surface");
                Require(parent == 0 || !tree.nodes.at(child).root, "invalid_relationship", "Root nodes can only attach to container 0");
                Require(owners.emplace(child, parent).second, "multiple_parents", "Final child has multiple owners");
                Require(plan.children.contains(child), "missing_children", "Every final node requires a complete child assignment, including leaves");
            }
        } catch (CommitError& error) {
            if (parent != 0) error.operationIndex = assignments.at(parent);
            throw;
        }
    }
    // With unique ownership, following parent links detects disconnected cycles too.
    std::unordered_set<int> checked;
    for (int id : parents) {
        std::unordered_set<int> path;
        int current = id;
        while (current != 0 && !checked.contains(current)) {
            Require(path.insert(current).second, "cycle", "Final ownership contains a cycle");
            const auto owner = owners.find(current);
            if (owner == owners.end()) break;
            current = owner->second;
        }
        checked.insert(path.begin(), path.end());
    }
    std::vector<int> pending(batch.rootChildren.rbegin(), batch.rootChildren.rend());
    while (!pending.empty()) {
        const int id = pending.back(); pending.pop_back();
        plan.ownedIds.insert(id);
        const auto& children = plan.children.at(id);
        pending.insert(pending.end(), children.rbegin(), children.rend());
    }
    for (size_t index = 0; index < batch.operations.size(); ++index)
        if (!plan.ownedIds.contains(batch.operations[index].id))
            throw CommitError("unreachable_operation", "Operations may target only final reachable nodes", index);

    // Previous-tree child order defines deterministic descendant-first cleanup.
    // Traverse surviving ancestors too: they may lose some of their descendants.
    std::vector<std::pair<int, bool>> deletion;
    for (auto it = tree.children[0].rbegin(); it != tree.children[0].rend(); ++it) deletion.emplace_back(*it, false);
    while (!deletion.empty()) {
        const auto [id, visited] = deletion.back(); deletion.pop_back();
        if (visited) {
            if (!plan.ownedIds.contains(id)) plan.destroyedIds.push_back(id);
        } else {
            deletion.emplace_back(id, true);
            const auto& children = tree.children.at(id);
            for (auto it = children.rbegin(); it != children.rend(); ++it) deletion.emplace_back(*it, false);
        }
    }
    return plan;
}

}
