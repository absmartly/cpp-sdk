# A/B Smartly SDK

A/B Smartly - C++ SDK

## Compatibility

The A/B Smartly C++ SDK is compatible with C++17 and later. It requires a compiler with C++17 support (GCC 7+, Clang 5+, MSVC 2017+). The SDK uses [nlohmann/json](https://github.com/nlohmann/json) for JSON handling, which is automatically fetched via CMake's FetchContent.

## Installation

#### CMake (FetchContent)

Add the following to your `CMakeLists.txt` to include the SDK directly from the repository:

```cmake
include(FetchContent)

FetchContent_Declare(
  absmartly-sdk
  GIT_REPOSITORY https://github.com/absmartly/cpp-sdk.git
  GIT_TAG main
)
FetchContent_MakeAvailable(absmartly-sdk)

target_link_libraries(your_target PRIVATE absmartly-sdk)
```

#### CMake (Local)

If you have cloned the repository locally, you can add it as a subdirectory:

```cmake
add_subdirectory(path/to/cpp-sdk)
target_link_libraries(your_target PRIVATE absmartly-sdk)
```

#### Building from Source

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

To build without tests:

```bash
cmake -DABSMARTLY_BUILD_TESTS=OFF ..
cmake --build .
```

## Getting Started

Please follow the [installation](#installation) instructions before trying the following code.

### Initialization

This example assumes an Api Key, an Application, and an Environment have been created in the A/B Smartly web console.

#### Recommended: Simple Initialization

```cpp
#include <absmartly/sdk.h>

int main() {
    auto sdk = absmartly::SDK::create_simple({
        .endpoint = "https://your-company.absmartly.io/v1",
        .api_key = "YOUR_API_KEY",
        .application = "website",
        .environment = "production"
    });

    absmartly::ContextConfig ctx_config;
    ctx_config.units = {{"session_id", "5ebf06d8cb5d8137290c4abb64155584fbdb64d8"}};

    auto context = sdk->create_context(ctx_config);
    context->wait_until_ready();

    return 0;
}
```

#### Alternative: Using Configuration Objects

For use cases with custom HTTP clients or providers:

```cpp
#include <absmartly/sdk.h>
#include <absmartly/client.h>
#include <absmartly/client_config.h>
#include <absmartly/sdk_config.h>
#include <absmartly/default_http_client.h>

int main() {
    absmartly::ClientConfig client_config;
    client_config.endpoint = "https://your-company.absmartly.io";
    client_config.api_key = "YOUR_API_KEY";
    client_config.application = "website";
    client_config.environment = "production";

    auto http_client = std::make_shared<absmartly::DefaultHTTPClient>();
    auto client = std::make_shared<absmartly::Client>(client_config, http_client);

    absmartly::SDKConfig sdk_config;
    sdk_config.client = client;

    auto sdk = absmartly::SDK::create(sdk_config);

    absmartly::ContextConfig ctx_config;
    ctx_config.units = {{"session_id", "5ebf06d8cb5d8137290c4abb64155584fbdb64d8"}};

    // Async context creation (fetches data from API)
    auto context = sdk->create_context(ctx_config);
    context->wait_until_ready();

    // Or with pre-fetched data
    auto data_future = sdk->get_context_data();
    auto data = data_future.get();
    auto context2 = sdk->create_context_with(ctx_config, data);

    return 0;
}
```

#### Direct Construction (without SDK wrapper)

```cpp
#include <absmartly/context.h>
#include <absmartly/context_config.h>
#include <absmartly/models.h>
#include <nlohmann/json.hpp>

int main() {
    std::string json_response = fetch_context_data(); // your HTTP client

    absmartly::ContextData data = nlohmann::json::parse(json_response)
        .get<absmartly::ContextData>();

    absmartly::ContextConfig config;
    config.units = {{"session_id", "5ebf06d8cb5d8137290c4abb64155584fbdb64d8"}};

    absmartly::Context context(config, data);

    return 0;
}
```

#### With Optional Parameters

```cpp
absmartly::ContextConfig config;
config.units = {{"session_id", "5ebf06d8cb5d8137290c4abb64155584fbdb64d8"}};
config.publish_delay = 100;     // delay before publishing in milliseconds
config.refresh_period = 0;      // auto-refresh period (0 = disabled)
config.overrides = {            // pre-set treatment overrides
    {"exp_test_experiment", 1}
};
config.custom_assignments = {   // pre-set custom assignments
    {"exp_another_experiment", 0}
};

absmartly::Context context(config, data);
```

#### With Event Handler

To handle SDK events, provide a custom event handler:

```cpp
#include <absmartly/context_event_handler.h>

class MyEventHandler : public absmartly::ContextEventHandler {
public:
    void handle_event(absmartly::Context& context,
                      const std::string& event_type,
                      const nlohmann::json& data) override {
        if (event_type == "exposure") {
            std::cout << "Exposed to: " << data["name"] << std::endl;
        } else if (event_type == "goal") {
            std::cout << "Goal tracked: " << data["name"] << std::endl;
        }
    }
};

// Pass the event handler as a shared_ptr
auto event_handler = std::make_shared<MyEventHandler>();
absmartly::Context context(config, data, event_handler);
```

**SDK Options (ContextConfig)**

| Config               | Type                              | Required? |   Default   | Description                                                                                                                                                                   |
| :------------------- | :-------------------------------- | :-------: | :---------: | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| units                | `std::map<std::string, std::string>` |  &#9989;  | `{}`        | A map of unit types to unit identifiers (e.g., `{{"session_id", "abc123"}}`).                                                                                                 |
| publish_delay        | `int`                             |  &#10060; | `-1`        | Delay in milliseconds before publishing events. Use `-1` to publish immediately.                                                                                               |
| refresh_period       | `int`                             |  &#10060; | `0`         | Period in milliseconds for automatic context refresh. Use `0` to disable.                                                                                                      |
| overrides            | `std::map<std::string, int>`      |  &#10060; | `{}`        | Pre-set treatment overrides for experiments.                                                                                                                                   |
| custom_assignments   | `std::map<std::string, int>`      |  &#10060; | `{}`        | Pre-set custom assignments for experiments.                                                                                                                                    |

## Creating a New Context

### Asynchronously (Recommended)

```cpp
auto sdk = absmartly::SDK::create_simple({
    .endpoint = "https://your-company.absmartly.io/v1",
    .api_key = "YOUR_API_KEY",
    .application = "website",
    .environment = "production"
});

absmartly::ContextConfig ctx_config;
ctx_config.units = {{"session_id", "5ebf06d8cb5d8137290c4abb64155584fbdb64d8"}};

auto context = sdk->create_context(ctx_config);
context->wait_until_ready();
```

### Direct Construction

You provide the context data obtained from your HTTP call to the A/B Smartly collector:

```cpp
absmartly::ContextConfig config;
config.units = {{"session_id", "5ebf06d8cb5d8137290c4abb64155584fbdb64d8"}};

absmartly::ContextData data = nlohmann::json::parse(json_response)
    .get<absmartly::ContextData>();

absmartly::Context context(config, data);
assert(context.is_ready());
```

### With Pre-fetched Data

Creating a context involves obtaining data from the A/B Smartly event collector. You can avoid repeating the round-trip by re-using previously retrieved data:

```cpp
absmartly::ContextConfig config;
config.units = {{"session_id", "5ebf06d8cb5d8137290c4abb64155584fbdb64d8"}};

absmartly::Context context(config, data);

absmartly::ContextConfig another_config;
another_config.units = {{"session_id", "another-session-id"}};

absmartly::Context another_context(another_config, context.data());
assert(another_context.is_ready());
```

### Refreshing the Context with Fresh Experiment Data

For long-running contexts, experiments started after the context was created will not be triggered. Call the `refresh()` method with updated data to incorporate new experiments:

```cpp
// Fetch fresh context data from the collector
absmartly::ContextData fresh_data = nlohmann::json::parse(fresh_json_response)
    .get<absmartly::ContextData>();

context.refresh(fresh_data);
```

### Setting Extra Units

You can add additional units to a context by calling the `set_unit()` or `set_units()` methods. This is useful when a user logs in and you want to associate the new identity with the context. Note that you cannot override an already set unit type, as that would be a change of identity and will throw an exception. In this case, you must create a new context instead.

```cpp
context.set_unit("db_user_id", "1000013");

context.set_units({
    {"db_user_id", "1000013"}
});
```

## Basic Usage

### Selecting a Treatment

```cpp
if (context.treatment("exp_test_experiment") == 0) {
    // user is in control group (variant 0)
} else {
    // user is in treatment group
}
```

### Treatment Variables

```cpp
nlohmann::json variable = context.variable_value("my_variable", nlohmann::json("default"));
```

Variables can be of any JSON type:

```cpp
nlohmann::json button_color = context.variable_value("button_color", "blue");
nlohmann::json show_banner = context.variable_value("show_banner", false);
nlohmann::json banner_height = context.variable_value("banner_height", 200);
```

### Peek at Treatment Variants

Although generally not recommended, it is sometimes necessary to peek at a treatment or variable without triggering an exposure. The A/B Smartly SDK provides `peek()` and `peek_variable_value()` methods for that.

```cpp
if (context.peek("exp_test_experiment") == 0) {
    // user is in control group (variant 0)
} else {
    // user is in treatment group
}
```

#### Peeking at Variables

```cpp
nlohmann::json variable = context.peek_variable_value("my_variable", nlohmann::json("default"));
```

### Overriding Treatment Variants

During development, for example, it is useful to force a treatment for an experiment. This can be achieved with the `set_override()` and/or `set_overrides()` methods.

```cpp
context.set_override("exp_test_experiment", 1);

context.set_overrides({
    {"exp_test_experiment", 1},
    {"exp_another_experiment", 0}
});
```

## Advanced

### Context Attributes

```cpp
context.set_attribute("user_agent", "Mozilla/5.0...");

context.set_attributes({
    {"customer_age", "new_customer"},
    {"url", "/products/123"}
});
```

### Custom Assignments

```cpp
context.set_custom_assignment("exp_test_experiment", 1);

context.set_custom_assignments({
    {"exp_test_experiment", 1},
    {"exp_another_experiment", 0}
});
```

### Custom Field Values

```cpp
nlohmann::json field_value = context.custom_field_value("exp_test_experiment", "my_field");
std::vector<std::string> field_keys = context.custom_field_keys();
```

### Variable Keys

```cpp
auto keys = context.variable_keys();
// Returns std::map<std::string, std::vector<std::string>>
// mapping variable names to the experiments that define them
```

### Tracking Goals

Goals are created in the A/B Smartly web console.

```cpp
context.track("payment", {
    {"item_count", 1},
    {"total_amount", 1999.99}
});
```

Track a goal without properties:

```cpp
context.track("page_view");
```

### Publishing Pending Data

Sometimes it is necessary to ensure all events have been published to the A/B Smartly collector, before proceeding. You can explicitly call the `publish()` method, which returns a `PublishEvent` containing all pending data.

```cpp
absmartly::PublishEvent event = context.publish();
// Serialize and send event to the A/B Smartly collector
nlohmann::json event_json = event;
send_to_collector(event_json.dump());
```

### Finalizing

The `finalize()` method will ensure all events have been published, like `publish()`, and will also "seal" the context, throwing a `ContextFinalizedException` if any method that could generate an event is called.

```cpp
absmartly::PublishEvent event = context.finalize();
// Send final event to collector
```

### Using RAII for Context Lifetime

In C++, you can use RAII patterns to ensure the context is properly finalized:

```cpp
{
    absmartly::Context context(config, data);

    int treatment = context.treatment("exp_test_experiment");
    // ... use treatment

    context.track("conversion");

    // Publish and send to collector before scope ends
    auto event = context.finalize();
    send_to_collector(nlohmann::json(event).dump());
} // context destroyed here
```

### Custom Event Handler

The A/B Smartly SDK can be instantiated with an event handler. Implement the `ContextEventHandler` interface to receive SDK lifecycle events.

```cpp
class CustomEventHandler : public absmartly::ContextEventHandler {
public:
    void handle_event(absmartly::Context& context,
                      const std::string& event_type,
                      const nlohmann::json& data) override {
        if (event_type == "exposure") {
            std::cout << "Exposed to experiment: " << data["name"] << std::endl;
        } else if (event_type == "goal") {
            std::cout << "Goal tracked: " << data["name"] << std::endl;
        } else if (event_type == "error") {
            std::cerr << "Error: " << data.dump() << std::endl;
        }
    }
};
```

Usage:

```cpp
auto handler = std::make_shared<CustomEventHandler>();
absmartly::Context context(config, data, handler);
```

**Event Types**

| Event      | When                                                       | Data                                   |
| ---------- | ---------------------------------------------------------- | -------------------------------------- |
| `error`    | `Context` receives an error                                | JSON object with error details         |
| `ready`    | `Context` is constructed and ready                         | JSON with experiment list              |
| `refresh`  | `Context::refresh()` method succeeds                       | JSON with updated experiment list      |
| `publish`  | `Context::publish()` method succeeds                       | `PublishEvent` serialized as JSON      |
| `exposure` | `Context::treatment()` succeeds on first exposure          | `Exposure` serialized as JSON          |
| `goal`     | `Context::track()` method succeeds                         | `GoalAchievement` serialized as JSON   |
| `finalize` | `Context::finalize()` method succeeds the first time       | empty JSON                             |

### Exception Types

The SDK defines the following exception types:

| Exception                    | When                                                      |
| ---------------------------- | --------------------------------------------------------- |
| `ContextFinalizedException`  | A method is called on a finalized or finalizing context   |
| `ContextNotReadyException`   | A method is called on a context that is not ready         |

## About A/B Smartly

**A/B Smartly** is the leading provider of state-of-the-art, on-premises, full-stack experimentation platforms for engineering and product teams that want to confidently deploy features as fast as they can develop them.
A/B Smartly's real-time analytics helps engineering and product teams ensure that new features will improve the customer experience without breaking or degrading performance and/or business metrics.

### Have a look at our growing list of clients and SDKs:
- [Java SDK](https://www.github.com/absmartly/java-sdk)
- [JavaScript SDK](https://www.github.com/absmartly/javascript-sdk)
- [PHP SDK](https://www.github.com/absmartly/php-sdk)
- [Swift SDK](https://www.github.com/absmartly/swift-sdk)
- [Vue2 SDK](https://www.github.com/absmartly/vue2-sdk)
- [Vue3 SDK](https://www.github.com/absmartly/vue3-sdk)
- [React SDK](https://www.github.com/absmartly/react-sdk)
- [Angular SDK](https://www.github.com/absmartly/angular-sdk)
- [Python3 SDK](https://www.github.com/absmartly/python3-sdk)
- [Go SDK](https://www.github.com/absmartly/go-sdk)
- [Ruby SDK](https://www.github.com/absmartly/ruby-sdk)
- [.NET SDK](https://www.github.com/absmartly/dotnet-sdk)
- [C++ SDK](https://www.github.com/absmartly/cpp-sdk) (this package)
- [Dart SDK](https://www.github.com/absmartly/dart-sdk)
- [Flutter SDK](https://www.github.com/absmartly/flutter-sdk)
