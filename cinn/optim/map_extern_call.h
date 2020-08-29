#pragma once

#include "cinn/ir/ir.h"

namespace cinn {
namespace optim {

static const std::set<std::string> kExternFp32Calls{
    {"exp",         "erf",         "sigmoid",     "sqrt",        "log",        "log2",        "log10",
     "floor",       "ceil",        "round",       "trunc",       "cos",        "cosh",        "tan",
     "sin",         "sinh",        "acos",        "acosh",       "asin",       "asinh",       "atan",
     "atanh",       "isnan",       "tanh",        "isfinite",    "isinf",      "left_shift",  "right_shift",
     "bitwise_or",  "bitwise_and", "bitwise_xor", "bitwise_not", "left_shift", "right_shift", "bitwise_or",
     "bitwise_and", "bitwise_xor", "bitwise_not"}};

static const std::set<std::string> kExternInt64Calls = {
    "left_shift", "right_shift", "bitwise_or", "bitwise_and", "bitwise_xor", "bitwise_not"};

/**
 * Map the Call nodes to external function call.
 *
 * This will rename the external call with the function in different backends.
 */
void MapExternCall(Expr *e, Target target);

}  // namespace optim
}  // namespace cinn
