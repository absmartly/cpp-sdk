#pragma once
// Shared helpers for the switchback context tests (ports of java-sdk ContextSwitchback*Test).
#include <catch2/catch_test_macros.hpp>
#include <absmartly/context.h>
#include <absmartly/context_event_logger.h>
#include <absmartly/hashing.h>
#include <absmartly/switchback.h>

#include "../src/clock.h"
#include "../src/switchback_state.h"

#include <functional>
#include <vector>

namespace sbtest {

using namespace absmartly;
using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

constexpr int64_t HOUR = 3600000;
constexpr int64_t DAY = 24 * HOUR;
// 2026-01-01T00:00:00Z, hour aligned.
constexpr int64_t ANCHOR = 1767225600000;

// The process clock seen by the SDK. next_read, when set, is returned once.
struct TestClock {
    static inline int64_t now = ANCHOR;
    static inline std::optional<int64_t> next_read;
    static int64_t read() {
        if (next_read) {
            const int64_t value = *next_read;
            next_read.reset();
            return value;
        }
        return now;
    }
};

// Installs the test clock for one test case.
struct ClockScope {
    explicit ClockScope(int64_t start = ANCHOR) {
        TestClock::now = start;
        TestClock::next_read.reset();
        internal::set_clock_for_testing(&TestClock::read);
    }
    ~ClockScope() { internal::set_clock_for_testing(nullptr); }
};

inline ordered_json clusters(std::initializer_list<std::pair<const char*, json>> entries) {
    ordered_json map = ordered_json::object();
    for (const auto& [type, path] : entries) map[type] = path;
    return map;
}

inline int64_t seed_of(int32_t hi, int32_t lo) {
    return static_cast<int64_t>((static_cast<uint64_t>(static_cast<uint32_t>(hi)) << 32) | static_cast<uint32_t>(lo));
}

struct ExposureLog : ContextEventLogger {
    std::vector<Exposure> exposures;
    std::function<void(Context&)> on_exposure;
    void handle_event(Context& context, const std::string& type, const json& data) override {
        if (type != "exposure") return;
        exposures.push_back(data.get<Exposure>());
        if (on_exposure) on_exposure(context);
    }
};

// Every exposure field that reaches the wire, exposedAt included.
inline json wire(const Exposure& e) {
    return {{"id", e.id}, {"name", e.name}, {"unit", e.unit}, {"variant", e.variant}, {"exposedAt", e.exposedAt},
            {"assigned", e.assigned}, {"eligible", e.eligible}, {"overridden", e.overridden}, {"fullOn", e.fullOn},
            {"custom", e.custom}, {"audienceMismatch", e.audienceMismatch}, {"ruleOverride", e.ruleOverride}};
}

struct Flags {
    bool assigned = true, eligible = true, overridden = false, full_on = false, custom = false,
         audience_mismatch = false, rule_override = false;
};

inline json exposure(int id, const std::string& name, const std::string& unit, int variant, int64_t at,
                     Flags f = {}) {
    return {{"id", id}, {"name", name}, {"unit", unit}, {"variant", variant}, {"exposedAt", at},
            {"assigned", f.assigned}, {"eligible", f.eligible}, {"overridden", f.overridden}, {"fullOn", f.full_on},
            {"custom", f.custom}, {"audienceMismatch", f.audience_mismatch}, {"ruleOverride", f.rule_override}};
}

inline json published(Context& context) {
    const auto event = context.publish();
    json result = json::array();
    for (const auto& e : event.exposures) result.push_back(wire(e));
    return result;
}

inline ContextData data_of(std::vector<ExperimentData> experiments, std::vector<HoldoutData> holdouts = {}) {
    ContextData data;
    data.experiments = std::move(experiments);
    data.holdouts = std::move(holdouts);
    return data;
}

inline ContextConfig units(std::initializer_list<std::pair<const char*, json>> entries) {
    ContextConfig config;
    for (const auto& [type, uid] : entries) config.units[type] = uid;
    return config;
}

}

namespace absmartly::internal {

// Test seam: the slot a switchback resolves to at the clock's current reading (java-sdk
// switchbackResolutionForTest).
struct ContextTestAccess {
    struct Resolution {
        std::string unit_type;
        bool assigned = false, eligible = false, full_on = false, rule_override = false, silent = false,
             applies = false;
    };
    static std::optional<Resolution> switchback(Context& context, const std::string& name) {
        if (!context.switchbacks_) return std::nullopt;
        const auto index = context.index_.find(name);
        if (index == context.index_.end() || !index->second.switchback) return std::nullopt;
        const auto epoch = context.switchbacks_->epochs.find(name);
        if (epoch == context.switchbacks_->epochs.end()) return std::nullopt;
        const auto slot = epoch->second.slots.find(index->second.switchback->period(internal::now_millis()));
        if (slot == epoch->second.slots.end()) return std::nullopt;
        const auto& a = slot->second.assignment;
        return Resolution{a.unit_type, a.assigned, a.eligible, a.full_on, a.rule_override, slot->second.silent,
                          slot->second.serving.status != internal::SwitchbackServing::Status::Absent};
    }
    static size_t switchback_slots(Context& context) {
        size_t n = 0;
        if (context.switchbacks_) {
            for (const auto& [name, epoch] : context.switchbacks_->epochs) n += epoch.slots.size();
        }
        return n;
    }
    static bool has_switchback_state(Context& context) { return context.switchbacks_ != nullptr; }
    // Slots dropped while a call was in progress, parked until the outermost call returns.
    static size_t retired_slots(Context& context) {
        return context.switchbacks_ ? context.switchbacks_->retired.size() : 0;
    }
};

}

namespace sbtest {
using absmartly::internal::ContextTestAccess;
}
