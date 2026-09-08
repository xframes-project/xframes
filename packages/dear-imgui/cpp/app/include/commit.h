#pragma once

#include <cstdint>
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

CommitBatch ParseCommit(nlohmann::json wire);
CommitResult UninitializedCommitResult();
nlohmann::json UninitializedCommitState();
int ParseNativeId(const nlohmann::json& value, bool allowContainer = false);
int ParseBindingId(double value, bool allowContainer = false);
PublicationPlan ValidatePublication(CommitBatch& batch, ValidationTree tree,
    const std::unordered_set<int>& managedIds, uint64_t nativeRevision);
void ValidateCommitProps(const std::string& type, const nlohmann::json& props, bool create);
bool IsCommitElementType(const std::string& type);
bool IsMeasuredElementType(const std::string& type);
}
