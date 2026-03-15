#pragma once
#include <stdexcept>
#include <string>

namespace absmartly {

class ContextFinalizedException : public std::runtime_error {
public:
    ContextFinalizedException() : std::runtime_error("ABsmartly Context is finalized.") {}
};

class ContextNotReadyException : public std::runtime_error {
public:
    ContextNotReadyException() : std::runtime_error("ABsmartly Context is not yet ready.") {}
};

} // namespace absmartly
