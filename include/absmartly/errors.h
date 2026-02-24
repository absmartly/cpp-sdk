#pragma once
#include <stdexcept>
#include <string>

namespace absmartly {

class ContextFinalizedException : public std::runtime_error {
public:
    ContextFinalizedException() : std::runtime_error("Context finalized") {}
};

class ContextNotReadyException : public std::runtime_error {
public:
    ContextNotReadyException() : std::runtime_error("Context not ready") {}
};

} // namespace absmartly
