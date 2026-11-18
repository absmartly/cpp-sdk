#pragma once
// A context's switchback slots; private to the SDK.
#include "absmartly/context.h"
#include "switchback_internal.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace absmartly::internal {

// Which unit serves a switchback and under which cluster key (the Collector's
// ContextAPI.switchbackServing).
struct SwitchbackServing {
    enum class Status { Absent, Ineligible, Eligible };
    Status status = Status::Absent;
    // The serving unit when Eligible, the first linked unit when Ineligible, empty when Absent.
    std::string unit_type;
    // The raw cluster key when Eligible; never hashed.
    std::string cluster_key;
};

// One experiment's resolution in one period, or in its one slot without a period (before the
// anchor, an unservable schedule, the legacy path).
struct SwitchbackSlot {
    Assignment assignment;
    // Read once per slot: a slot rebuilt in the same epoch keeps it, unless no linked unit was set.
    SwitchbackServing serving;
    // The custom assignment read when the slot was resolved, consulted or not.
    std::optional<int> custom;
    // Nothing is recorded for it: no linked unit, an ineligible cluster, or no draw. Such a slot
    // is created already exposed.
    bool silent = false;
};

struct SwitchbackState {
    using Slots = std::map<int64_t, SwitchbackSlot>;
    // The slots of one experiment resolved under one generation of its definition.
    struct Epoch {
        uint64_t generation = 0;
        Slots slots;
    };

    std::map<std::string, Epoch, std::less<>> epochs;
    uint64_t generation_counter = 0;
    // treatment() and variable_value() calls in progress. Each holds a slot across the event
    // logger, which may call back in and drop it, so a slot dropped meanwhile is parked in
    // retired until the outermost call returns.
    int depth = 0;
    std::vector<Slots::node_type> retired;
    std::vector<std::map<std::string, Assignment>::node_type> retired_assignments;

    void drop(Slots& slots, Slots::iterator it) {
        if (depth > 0) {
            retired.push_back(slots.extract(it));
        } else {
            slots.erase(it);
        }
    }
};

// Counts a public call in progress for SwitchbackState::depth.
class SwitchbackCallGuard {
public:
    explicit SwitchbackCallGuard(SwitchbackState* state) : state_(state) {
        if (state_) ++state_->depth;
    }
    ~SwitchbackCallGuard() {
        if (state_ && --state_->depth == 0) {
            state_->retired.clear();
            state_->retired_assignments.clear();
        }
    }
    SwitchbackCallGuard(const SwitchbackCallGuard&) = delete;
    SwitchbackCallGuard& operator=(const SwitchbackCallGuard&) = delete;

private:
    SwitchbackState* state_;
};

} // namespace absmartly::internal
