#pragma once

#include "cbasic_common.hpp"
#include <vector>
#include <functional>

// ============================================================================
// ARRAY SUBSYSTEM
// ============================================================================
// Multi-dimensional array storage for the CrossBASIC VM. Arrays are stored
// flattened (row-major) with per-dimension extents. Indexing honors OPTION BASE
// and is bounds- and overflow-checked.

struct ArrayData {
    std::vector<int> extents;
    std::vector<Value> data;
};

// Flatten a multi-dimensional index into a linear offset. `indices` must have
// exactly one entry per dimension. Returns true on success and sets `linearOut`;
// on failure returns false and sets `errorOut`.
bool flattenArrayIndexChecked(
    const ArrayData& arr,
    const std::vector<int>& indices,
    int optionBase,
    int& linearOut,
    std::string& errorOut);

// Build an ArrayData from per-dimension upper bounds (BASIC semantics: the
// upper bound is inclusive, so extent = bound - optionBase + 1). Validates
// bounds and total size. Returns true on success; on failure sets `errorOut`.
bool makeArrayFromUpperBoundsChecked(
    const std::vector<int>& upperBounds,
    int optionBase,
    ArrayData& outArr,
    std::string& errorOut);

// Build an implicitly-dimensioned array (default upper bound 10 per dimension).
bool makeImplicitArrayChecked(
    int dims,
    int optionBase,
    ArrayData& outArr,
    std::string& errorOut);
