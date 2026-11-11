# Changelog

## Unreleased

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
