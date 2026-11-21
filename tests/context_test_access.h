#pragma once

#include <absmartly/context.h>

namespace absmartly {

struct ContextTestAccess {
    static Assignment& assignment(Context& ctx, const char* name) { return ctx.assignments_.at(name); }
    static int data_seq(const Context& ctx) { return ctx.data_seq_; }
    static const nlohmann::json& attributes_map(const Context& ctx) { return ctx.get_attributes_map(); }
    static nlohmann::json& attributes_map_cache(Context& ctx) { return ctx.attrs_map_cache_; }
    static size_t walk_capacity(const Context& ctx) { return ctx.walk_generations_.capacity(); }
    static std::uint64_t index_lookups(const Context& ctx) { return ctx.index_lookups_; }
    static size_t retired(const Context& ctx) { return ctx.retired_.size(); }
};

} // namespace absmartly
