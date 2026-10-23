#pragma once
#include <stdexcept>
#include <string>

namespace absmartly {

class ContextFinalizedException : public std::runtime_error {
public:
    ContextFinalizedException() : std::runtime_error("ABsmartly Context is finalized.") {}

protected:
    explicit ContextFinalizedException(const char* message) : std::runtime_error(message) {}
};

// Thrown while finalize() is still flushing; a ContextFinalizedException so
// existing handlers keep catching it.
class ContextFinalizingException : public ContextFinalizedException {
public:
    ContextFinalizingException() : ContextFinalizedException("ABsmartly Context is finalizing.") {}
};

class ContextNotReadyException : public std::runtime_error {
public:
    ContextNotReadyException() : std::runtime_error("ABsmartly Context is not yet ready.") {}
};

} // namespace absmartly
