#include "context.h"

context::context() {
    if (context_ = interception_create_context(); context_ == 0) {
        throw std::runtime_error("failed to create interception context");
    }
}

context::~context() {
    interception_destroy_context(context_);
}
