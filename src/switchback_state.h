#pragma once
// A context's switchback slots; private to the SDK.
#include "absmartly/context.h"
#include "switchback_internal.h"

#include <map>
#include <memory>
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
    // Extracted nodes are reinserted into owning multimaps: references obtained before extraction
    // are usable again only after insertion, not while a node handle holds them.
    std::multimap<int64_t, SwitchbackSlot> retired;
    std::multimap<std::string, Assignment> retired_assignments;

    template <typename Map, typename Parked>
    static void retire(Map& map, typename Map::iterator it, Parked& parked, bool in_call) {
        if (in_call) {
            parked.insert(map.extract(it));
        } else {
            map.erase(it);
        }
    }

    void drop(Slots& slots, Slots::iterator it, bool in_call) {
        retire(slots, it, retired, in_call);
    }
    void drain(Epoch& epoch, bool in_call) {
        while (!epoch.slots.empty()) drop(epoch.slots, epoch.slots.begin(), in_call);
    }
    // Out of line: the teardown of parked maps stays off the callers' inlined fast path.
    void release_retired();
};

// Counts a treatment() or variable_value() call in progress. The count lives on the Context,
// so a call that starts before the first switchback is loaded (a logger may refresh one in) is
// counted too.
class SwitchbackCallGuard {
public:
    SwitchbackCallGuard(int& depth, const std::shared_ptr<SwitchbackState>& state) : depth_(depth), state_(state) {
        ++depth_;
    }
    ~SwitchbackCallGuard() {
        if (--depth_ == 0 && state_ && !(state_->retired.empty() && state_->retired_assignments.empty())) {
            state_->release_retired();
        }
    }
    SwitchbackCallGuard(const SwitchbackCallGuard&) = delete;
    SwitchbackCallGuard& operator=(const SwitchbackCallGuard&) = delete;

private:
    int& depth_;
    const std::shared_ptr<SwitchbackState>& state_;
};

}
