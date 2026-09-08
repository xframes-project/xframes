#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace xframes {
enum class CommitOp { Create, Patch, SetChildren, AppendChild };

// Owned values only: no Elements, Yoga nodes, widget resources or borrowed JSON.
struct CommitOperation {
    CommitOp op;
    int id = 0; // target, or parent for child operations
    std::string elementType;
    nlohmann::json props = nlohmann::json::object();
    std::vector<int> children;
    bool skip = false; // validation marks accepted stale compatibility no-ops
};

struct CommitBatch {
    std::optional<std::string> correlationId;
    std::vector<CommitOperation> operations;
    bool compatibility = false; // native-only; never accepted on the wire
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

CommitBatch ParseCommit(nlohmann::json wire);
CommitResult UninitializedCommitResult();
nlohmann::json UninitializedCommitState();
int ParseNativeId(const nlohmann::json& value, bool allowContainer = false);
int ParseBindingId(double value, bool allowContainer = false);
std::vector<int> ParseChildrenIds(std::string_view wire);
void ValidateCommit(CommitBatch& batch, ValidationTree tree);
void ValidateCommitProps(const std::string& type, const nlohmann::json& props, bool create);
bool IsCommitElementType(const std::string& type);
bool IsMeasuredElementType(const std::string& type);
}
