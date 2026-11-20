# Changelog

## 1.1.0 (unreleased)

### Added

- Holdouts (`ContextData::holdouts`, `HoldoutData`, `ExperimentData::holdoutIds`
  and `ExperimentData::holdoutType`, all new fields appended last and empty by
  default). An experiment covered by a holdout that holds the unit out
  (arm 0, or arm 1 of a three-arm holdout for an experiment that is not
  full-on) returns variant 0 with control variables and publishes no exposure
  of its own; each covering holdout publishes one ordinary exposure per unit
  type. Overrides and matching assignment rules win over a holdout. Payloads
  without holdouts assign and publish exactly as before. Ports javascript-sdk#65
  and java-sdk#12.

### Fixed

- An event logger that called `refresh()` from an exposure inside
  `variable_value()` could crash the call (the candidate list it walked was
  freed). The walk now continues over the candidates it started with, as in
  javascript-sdk.
- An event logger that refreshed the context from an exposure and read the same
  experiment again could change what the outer `treatment()`/`variable_value()`
  returned; both now return the decision they exposed, as in javascript-sdk.

### Notes

- New fields grow the public model and `Context` layouts. The package stays
  source compatible within 1.x; rebuild everything that includes the SDK
  headers when upgrading (see Installed Package in the README).

- All exposures of one `treatment()`/`variable_value()` call on a covered
  experiment (its own and its holdouts') are queued before the event logger
  sees the first of them, so a logger that calls `finalize()` or `publish()`
  from that callback sends them all. javascript-sdk queues each one just
  before its callback, and there such a logger leaves the holdout exposures
  pending.

## 1.0.0

### Added

- Ordered assignment rules (`ExperimentData::assignmentRules`). The first
  matching rule decides the variant, taking precedence over audience targeting,
  `audienceStrict`, traffic allocation, full-on and custom assignments; only
  `set_override()` beats it. An `assign` rule gives a fixed variant and a
  `split` rule (`"20/30/50"`) randomises the unit with the experiment's seed.
  Rules can be scoped to environments, and rule-assigned exposures carry
  `ruleOverride: true` so the backend can exclude them from analysis. If the
  first matching rule's variant is out of range, or its split does not have one
  percentage per variant, normal assignment applies and later rules are not
  tried. Ports javascript-sdk#66 and java-sdk#17.
  An `assignmentRules` value that is not a JSON object means no rules for that
  experiment, without an `error` event (JS and Java only log it).
- `ContextConfig::application` / `environment`, the system attribute names for
  a `Context` constructed directly. `SDK::create_context()` /
  `create_context_with()` always set them from the client config, the names the
  collector scopes by.
- `semver_eq`, `semver_gt`, `semver_gte`, `semver_lt` and `semver_lte` operators
  for audience filters and rule conditions.
- `<absmartly/sdk_version.h>`, generated from the CMake project version
  (`SDK_NAME`, `SDK_VERSION`, `SDK_AGENT`).
- CMake install/export (`find_package(absmartly-sdk)`, target
  `absmartly::absmartly-sdk`), CPack `.tar.gz`/`.zip` archives, and a release
  workflow that builds, tests and attaches them to a GitHub release on `v*`
  tags. The install and package rules are generated only when the SDK is the
  top-level project (option `ABSMARTLY_INSTALL`), so a project that embeds it
  with `FetchContent`/`add_subdirectory` keeps its own install tree and CPack
  configuration.

### Fixed

- A matching rule that won over a different custom assignment was treated as
  stale and rebuilt (re-queuing its exposure) on every `treatment()` call.

### Changed

- `X-Agent` is now `absmartly-cpp-sdk/<version>` (was `absmartly-cpp-sdk`).
  This changes the `agent` value recorded for your requests, and so the agent
  column in data exports. A build that compiles the sources without CMake has
  no generated version header and keeps sending `absmartly-cpp-sdk`.

- Audiences and assignment-rule conditions are now always evaluated with the
  `application` and `environment` names from the client configuration. The SDK
  injects them into the attribute map it evaluates audiences against (system
  values first, with any user-set attribute of the same name taking
  precedence), skipping either key when its name is empty. This is always on,
  with no opt-out, matching the behaviour the collector already applies
  server-side.

  **This changes assignment for running experiments.** A unit whose experiment
  has Audience enforced (`audienceStrict`) and an `environment` or `application`
  condition currently gets variant 0, because the SDK never put those values in
  the evaluation map and the missing attribute failed the filter. Those
  experiments are broken today. After upgrading, such a unit is randomised
  normally.

  Experiments with a **non-strict** audience on `environment` or `application`
  keep their variants (the audience never affected their assignment), but their
  exposures stop being flagged `audienceMismatch` when the names match.

- `app_version` / `application_version` is not injected as an evaluation
  attribute. A filter on `app_version` does not match even when an application
  version is configured. (The C++ SDK never added it to the evaluation map or
  the publish payload, so no payload changes result from this.)

### Notes

- Nothing new is added to the publish payload: `application` and `environment`
  are evaluation-only here, since the collector already attaches both to every
  exposure server-side. User-set attributes continue to be published as before.
