#pragma once

#include <stdexcept>

#include "interception.h"

class context {
public:
    context();
    ~context();

    inline operator InterceptionContext() const noexcept { return context_; }

private:
    InterceptionContext context_;
};
