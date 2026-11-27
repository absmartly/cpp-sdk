# Changelog

## Unreleased

### Fixed

- **Shared variable keys exposed experiments the user never saw.** When two or
  more experiments define the same variable key, `variable_value()` queued an
  exposure for every candidate it visited on the way to the winner, including
  experiments the unit was not in (a strict audience mismatch, no unit for the
  experiment's unit type) and experiments that ended on the caller default.
  Those users were recorded in experiments that never affected them. Both
  `variable_value()` and `peek_variable_value()` could also return a value from
  a later experiment when the winner's assigned arm did not define the key, so
  the arm a user got decided which experiment they were counted in. This
  affects anyone with two or more running experiments that share a variable
  key. The rule now applied, matching the Java SDK: the candidates are the
  experiments whose variants define the key, by experiment id ascending; the
  winner is the first one the unit is assigned to (including a unit outside the
  traffic split, which gets the control value), overridden into or
  rule-assigned to; if the winner's variant does not define the key the caller
  default is returned and no later experiment is tried; if no candidate
  qualifies the default is returned. Only the winner is exposed, and
  `peek_variable_value()` exposes nothing. This replaces the earlier rule
  (shared with the JavaScript SDK) of exposing every candidate checked and
  falling through to a later experiment when the key was missing.

  This also applies when only one experiment defines the key: a unit that is
  not in it (a strict audience mismatch, or no unit for its unit type) gets the
  default and is no longer exposed by `variable_value()`, so exposure counts
  for such experiments can drop. `treatment()` still exposes it. An override to
  a variant the experiment does not have now makes that experiment the winner
  with the default value and an exposure, instead of falling through to a
  later experiment's value. An override set before its experiment is loaded
  now binds to that experiment once a refresh brings it in, so the value and
  the exposure (id, unit type) come from the loaded experiment, whatever its
  id; if it was already exposed before the refresh, it is exposed once more
  with that data. An override already bound to an experiment that leaves and
  returns unchanged is not exposed again. `variable_value()` and `treatment()`
  return the value and variant they exposed even when an exposure event
  handler changes overrides or refreshes the context.

## 1.0.0 (2026-11-16)

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
