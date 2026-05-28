#pragma once

#include <exception>
#include <functional>

namespace poker {

class operation_cancelled : public std::exception {
 public:
    const char* what() const noexcept override { return "operation cancelled"; }
};

using CancelPredicate = std::function<bool()>;

inline void throw_if_cancelled(const CancelPredicate* cancel) {
    if (cancel && (*cancel)()) {
        throw operation_cancelled{};
    }
}

}  // namespace poker
