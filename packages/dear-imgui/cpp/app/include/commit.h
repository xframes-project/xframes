#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <nlohmann/json.hpp>

namespace xframes {
enum class CommitOp { Create, Patch, SetChildren };

// Owned values only: no Elements, Yoga nodes, widget resources or borrowed JSON.
struct CommitOperation {
    CommitOp op;
    int id = 0; // target, or parent for child operations
    std::string elementType;
    nlohmann::json props = nlohmann::json::object();
    std::vector<int> children;
};

struct CommitBatch {
    uint64_t baseRevision = 0; // schema 2 optimistic structural revision
    std::vector<int> rootChildren; // schema 2 complete virtual-root list
    std::optional<std::string> correlationId;
    std::vector<CommitOperation> operations;
    // "childLists": "changed": an omitted child list is the previous one, and a created
    // node without an assignment has none. The default, "complete", needs every list.
    bool changedChildLists = false;
};

struct CommitError : std::runtime_error {
    std::string code;
    std::optional<size_t> operationIndex;
    CommitError(std::string code, std::string message, std::optional<size_t> index = std::nullopt)
        : std::runtime_error(std::move(message)), code(std::move(code)), operationIndex(index) {}
};

struct CommitResult {
    std::string status = "rejected";
    std::optional<uint64_t> nativeSequence;
    uint64_t nativeRevision = 0;
    std::optional<std::string> correlationId;
    std::vector<int> destroyedIds;
    std::optional<CommitError> error;
    nlohmann::json ToJson() const;
};

struct ValidationNode {
    std::string type;
    bool root = false;
    bool measured = false;
};
struct ValidationTree {
    std::unordered_map<int, ValidationNode> nodes;
    std::unordered_map<int, std::vector<int>> children;
};

// The complete, validated surface. Owned only by the synchronous request.
struct PublicationPlan {
    std::unordered_map<int, std::vector<int>> children;
    std::unordered_set<int> ownedIds;
    std::vector<int> destroyedIds; // previous tree postorder, excluding survivors
};

// What a changed-list publication does to the applied surface. Owned only by the request.
struct ChangedPublicationPlan {
    std::vector<std::pair<int, std::vector<int>>> assigned; // container 0 first, then assigned parents
    std::vector<int> created;      // every created node; all are finally reachable
    std::vector<int> destroyedIds; // descendant-first in the previous tree
};

// The applied surface as changed-list validation reads it. Nothing is copied.
struct AppliedSurface {
    const std::unordered_map<int, std::vector<int>>& children;
    const std::unordered_map<int, int>& parentOf; // publication-owned nodes; roots map to 0
    std::function<std::optional<ValidationNode>(int)> node;
};

CommitBatch ParseCommit(nlohmann::json wire);
CommitResult UninitializedCommitResult();
nlohmann::json UninitializedCommitState();
int ParseNativeId(const nlohmann::json& value, bool allowContainer = false);
int ParseBindingId(double value, bool allowContainer = false);
PublicationPlan ValidatePublication(CommitBatch& batch, ValidationTree tree,
    const std::unordered_set<int>& managedIds, uint64_t nativeRevision);
// Validates the final tree a changed-list batch describes over the applied surface, with
// the same rules as ValidatePublication except that omitted lists are kept. Its work
// follows the operations, the assigned lists and the nodes that leave.
ChangedPublicationPlan ValidateChangedPublication(CommitBatch& batch, const AppliedSurface& surface,
    const std::unordered_set<int>& managedIds, uint64_t nativeRevision);
void ValidateCommitProps(const std::string& type, const nlohmann::json& props, bool create);
bool IsCommitElementType(const std::string& type);
bool IsMeasuredElementType(const std::string& type);
}
