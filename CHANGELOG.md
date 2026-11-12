# Changelog

## Unreleased

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
- `semver_eq`, `semver_gt`, `semver_gte`, `semver_lt` and `semver_lte` operators
  for audience filters and rule conditions.
- `X-Agent: absmartly-cpp-sdk/<version>` (was `absmartly-cpp-sdk`), with the
  version generated from the CMake project version into
  `<absmartly/sdk_version.h>`.
- CMake install/export (`find_package(absmartly-sdk)`, target
  `absmartly::absmartly-sdk`), CPack `.tar.gz`/`.zip` archives, and a release
  workflow that builds, tests and attaches them to a GitHub release on `v*`
  tags.

### Fixed

- A matching rule that won over a different custom assignment was treated as
  stale and rebuilt (re-queuing its exposure) on every `treatment()` call.

### Changed

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

- `app_version` / `application_version` is not injected as an evaluation
  attribute. A filter on `app_version` does not match even when an application
  version is configured. (The C++ SDK never added it to the evaluation map or
  the publish payload, so no payload changes result from this.)

### Notes

- Nothing new is added to the publish payload: `application` and `environment`
  are evaluation-only here, since the collector already attaches both to every
  exposure server-side. User-set attributes continue to be published as before.
