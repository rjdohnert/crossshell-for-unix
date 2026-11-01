#include "cbasic_array.hpp"
#include <limits>
#include <new>

bool flattenArrayIndexChecked(
    const ArrayData& arr,
    const std::vector<int>& indices,
    int optionBase,
    int& linearOut,
    std::string& errorOut)
{
    linearOut = 0;
    if (indices.size() != arr.extents.size()) {
        errorOut = "Array dimension mismatch";
        return false;
    }

    // 64-bit accumulation with overflow checks: 32-bit int can overflow before
    // the size_t bounds check when extents are large.
    int64_t linear = 0;
    int64_t stride = 1;
    for (int i = static_cast<int>(arr.extents.size()) - 1; i >= 0; --i) {
        int internalIdx = indices[i] - optionBase;
        if (internalIdx < 0 || internalIdx >= arr.extents[i]) {
            errorOut = "Array index out of bounds";
            return false;
        }
        linear += static_cast<int64_t>(internalIdx) * stride;
        stride *= static_cast<int64_t>(arr.extents[i]);
        if (linear > static_cast<int64_t>(std::numeric_limits<int>::max()) ||
            stride > static_cast<int64_t>(std::numeric_limits<int>::max())) {
            errorOut = "Array index is too large";
            return false;
        }
    }
    linearOut = static_cast<int>(linear);
    return true;
}

bool makeArrayFromUpperBoundsChecked(
    const std::vector<int>& upperBounds,
    int optionBase,
    ArrayData& outArr,
    std::string& errorOut)
{
    try {
        outArr.extents.reserve(upperBounds.size());

        size_t total = 1;
        for (int bound : upperBounds) {
            if (bound < optionBase) {
                errorOut = "Array upper bound is below OPTION BASE";
                return false;
            }
            int extent = (bound - optionBase) + 1;
            if (extent <= 0) {
                errorOut = "Array upper bound is invalid";
                return false;
            }
            if (total > (std::numeric_limits<size_t>::max() / static_cast<size_t>(extent))) {
                errorOut = "Array is too large";
                return false;
            }
            outArr.extents.push_back(extent);
            total *= static_cast<size_t>(extent);
        }
        outArr.data.assign(total, 0.0);
    } catch (const std::bad_alloc&) {
        errorOut = "Out of memory while allocating array";
        return false;
    }
    return true;
}

bool makeImplicitArrayChecked(
    int dims,
    int optionBase,
    ArrayData& outArr,
    std::string& errorOut)
{
    int defaultUpper = 10;
    int extent = (defaultUpper >= optionBase) ? ((defaultUpper - optionBase) + 1) : 1;
    try {
        outArr.extents.assign(dims, extent);

        size_t total = 1;
        for (int i = 0; i < dims; ++i) {
            if (total > (std::numeric_limits<size_t>::max() / static_cast<size_t>(extent))) {
                errorOut = "Array is too large";
                return false;
            }
            total *= static_cast<size_t>(extent);
        }
        outArr.data.assign(total, 0.0);
    } catch (const std::bad_alloc&) {
        errorOut = "Out of memory while allocating array";
        return false;
    }
    return true;
}
