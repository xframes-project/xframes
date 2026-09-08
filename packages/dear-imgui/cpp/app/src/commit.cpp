#include "commit.h"
#include <algorithm>
#include <climits>
#include <cmath>
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
    json result = {{"schemaVersion", 1}, {"surfaceId", 0}, {"status", status},
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
    return {{"schemaVersion", 1}, {"surfaceId", 0}, {"initialized", false},
        {"nativeSequence", "0"}, {"nativeRevision", "0"}};
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

std::vector<int> ParseChildrenIds(std::string_view wire) {
    const auto parsed = json::parse(wire);
    Require(parsed.is_array(), "invalid_field", "childrenIds must be an array");
    std::vector<int> result;
    for (const auto& id : parsed) result.push_back(ParseNativeId(id));
    return result;
}

CommitBatch ParseCommit(json wire) {
    Fields(wire, {"schemaVersion", "surfaceId", "operations"}, {"correlationId"});
    Require(wire["schemaVersion"].is_number_integer() && wire["schemaVersion"] == 1,
        "unsupported_version", "Only schemaVersion 1 is supported");
    Require(wire["surfaceId"].is_number_integer() && wire["surfaceId"] == 0,
        "unsupported_surface", "Only surfaceId 0 is supported");
    Require(wire["operations"].is_array(), "invalid_field", "operations must be an array");
    CommitBatch batch;
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
            } else if (name == "setChildren" || name == "appendChild") {
                op.op = name == "setChildren" ? CommitOp::SetChildren : CommitOp::AppendChild;
                if (op.op == CommitOp::SetChildren) {
                    Fields(def, {"op", "parentId", "childrenIds"});
                    Require(def["childrenIds"].is_array(), "invalid_field", "childrenIds must be an array");
                    for (auto& id : def["childrenIds"]) op.children.push_back(ParseNativeId(id));
                } else {
                    Fields(def, {"op", "parentId", "childId"});
                    op.children.push_back(ParseNativeId(def["childId"]));
                }
                op.id = ParseNativeId(def["parentId"], true);
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

void ValidateCommit(CommitBatch& batch, ValidationTree tree) {
    std::unordered_set<int> destroyed;
    // An iterative validation-only traversal avoids allocating any live native resources.
    auto remove = [&](int id) {
        std::vector<int> pending{id};
        while (!pending.empty()) {
            int current = pending.back(); pending.pop_back();
            if (!destroyed.insert(current).second) continue;
            auto it = tree.children.find(current);
            if (it != tree.children.end()) {
                pending.insert(pending.end(), it->second.begin(), it->second.end());
                tree.children.erase(it);
            }
            tree.nodes.erase(current);
        }
    };
    for (size_t index = 0; index < batch.operations.size(); ++index) {
        auto& op = batch.operations[index];
        try {
            if (op.op == CommitOp::Create) {
                Require(!destroyed.contains(op.id), "destroyed_id", "Cannot recreate an ID destroyed in this transaction");
                Require(!tree.nodes.contains(op.id), "duplicate_id", "Create target already exists");
                Require(IsCommitElementType(op.elementType), "invalid_element_type", "Unsupported elementType");
                ValidateCommitProps(op.elementType, op.props, true);
                tree.nodes.emplace(op.id, ValidationNode{op.elementType, op.props.value("root", false), IsMeasuredElementType(op.elementType)});
                tree.children[op.id] = {};
                continue;
            }
            const bool missing = op.id != 0 && !tree.nodes.contains(op.id);
            if (missing && batch.compatibility) { op.skip = true; continue; }
            Require(!missing, destroyed.contains(op.id) ? "destroyed_id" : "missing_target", "Operation target is not live");
            if (op.op == CommitOp::Patch) {
                op.elementType = tree.nodes.at(op.id).type;
                if (batch.compatibility) op.props.erase("root"); // legacy clone metadata, never a root mutation
                ValidateCommitProps(op.elementType, op.props, false);
                continue;
            }
            if (op.op == CommitOp::AppendChild && batch.compatibility &&
                (!tree.children.contains(op.id) || !tree.nodes.contains(op.children.front()))) {
                op.skip = true; continue;
            }
            auto children = op.children;
            if (op.op == CommitOp::AppendChild) {
                children = tree.children[op.id];
                if (std::find(children.begin(), children.end(), op.children.front()) != children.end()) {
                    op.skip = true; continue; // idempotent append in both APIs
                }
                children.push_back(op.children.front());
            }
            std::unordered_set<int> unique;
            for (int child : children) {
                const bool first = unique.insert(child).second;
                Require(first || (batch.compatibility && op.id == 0), "duplicate_child", "A child list cannot contain duplicate IDs");
                Require(child != op.id, "cycle", "A node cannot contain itself");
                if (!batch.compatibility) {
                    Require(tree.nodes.contains(child), destroyed.contains(child) ? "destroyed_id" : "missing_target", "Child is not live (forward references are unsupported)");
                    Require(op.id == 0 || !tree.nodes.at(child).root, "invalid_relationship", "Root nodes can only attach to container 0");
                    for (const auto& [parent, siblings] : tree.children) {
                        Require(parent == op.id || std::find(siblings.begin(), siblings.end(), child) == siblings.end(),
                            "multiple_parents", "Child already belongs to another parent");
                    }
                }
                std::vector<int> pending{child};
                std::unordered_set<int> visited;
                while (!pending.empty()) {
                    int current = pending.back(); pending.pop_back();
                    Require(current != op.id, "cycle", "Child relationship would create a cycle");
                    if (!visited.insert(current).second) continue;
                    auto it = tree.children.find(current);
                    if (it != tree.children.end()) pending.insert(pending.end(), it->second.begin(), it->second.end());
                }
            }
            Require(children.empty() || op.id == 0 || !tree.nodes.at(op.id).measured,
                "invalid_relationship", "A measured Yoga leaf cannot own children");
            if (op.op == CommitOp::SetChildren) {
                const auto old = tree.children[op.id];
                for (int child : old) if (!unique.contains(child)) remove(child);
                // Removing an ancestor also destroys any descendant the new list tried to retain.
                if (!batch.compatibility) for (int child : children)
                    Require(tree.nodes.contains(child), "destroyed_id", "Child was destroyed by this operation's removal");
            }
            tree.children[op.id] = std::move(children);
        } catch (CommitError& error) { error.operationIndex = index; throw; }
    }
}
}
