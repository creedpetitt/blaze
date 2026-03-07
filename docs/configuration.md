# Configuration Guide

Blaze offers a fluent, type-safe API for configuring your application. This guide covers all available settings and best practices for production deployments.

---

## The App Builder API

Instead of passing a long list of parameters to a constructor, Blaze uses a **Fluent Interface** (also known as the Builder pattern) on the `App` class. This allows you to chain configuration methods together.

```cpp
#include <blaze/app.h>

using namespace blaze;

int main() {
    App app;

    app.server_name("Blaze-API/1.0")    // Custom 'Server' HTTP header
       .max_body_size(10 * 1024 * 1024) // 10MB limit
       .timeout(30)                     // 30s request timeout
       .num_threads(4)                  // Event loop thread count
       .enable_docs(true);              // Toggle Swagger UI (/docs)

    app.listen(8080);
}
```

### Configuration Options

| Method | Default | Description |
| :--- | :--- | :--- |
| `.server_name(string)` | `"Blaze/1.0"` | The string sent in the `Server` response header. |
| `.max_body_size(bytes)` | `10MB` | The maximum size of an HTTP request body. Larger requests return `413 Payload Too Large`. |
| `.timeout(seconds)` | `30` | The time Blaze waits for a request to complete before closing the connection. |
| `.num_threads(int)` | `auto` | Number of CPU threads to use for the event loop. `0` auto-detects based on hardware. |
| `.enable_docs(bool)` | `true` | Whether to register the `/docs` (Swagger UI) and `/openapi.json` routes. |
| `.shutdown_timeout(sec)`| `30` | Grace period for active connections during server shutdown. |

> **Note on Static Files**: Blaze uses zero-copy streaming for all static assets. This means the server does not need a "Cache Size" configuration for file contents, as it leverages the OS Page Cache for maximum performance without memory bloat.

---

## Logging

Blaze uses an asynchronous logger that processes messages in a background thread to prevent blocking your high-performance request handlers.

### Log Levels

You can set the minimum severity level using `.log_level(LogLevel)`. Available levels are:
*   `LogLevel::DEBUG`: Verbose output for development.
*   `LogLevel::INFO`: General application flow (Default).
*   `LogLevel::WARN`: Potential issues that don't stop the app.
*   `LogLevel::ERROR`: Serious failures and exceptions.

```cpp
app.log_level(LogLevel::DEBUG);
```

### Log Targets

By default, Blaze logs to `stdout`. In production, you can redirect this to a file.

```cpp
// Log to a file
app.log_to("server.log");
```

> **Note on Lazy Logging**: To keep your workspace clean, Blaze uses "Lazy Logging." The log file will not be physically created on your disk until the first log message is actually generated. If no traffic hits your server, no file will appear.

---

## Lifecycle & Shutdown

Blaze ensures that your application shuts down cleanly without dropping in-flight requests.

| Method | Default | Description |
| :--- | :--- | :--- |
| `.shutdown_timeout(sec)`| `30` | The maximum time to wait for active requests to finish before force-killing the server. |

**Optimized Behavior**: 
As of v1.1.0, the shutdown process is highly optimized. If your server has zero active connections, it will shut down **instantly** upon receiving a stop signal. The timeout only acts as a safety net for hung requests.

While the fluent API is great for code-based config, sensitive data like API keys should stay in `.env` files.

```cpp
#include <blaze/environment.h>

int main() {
    blaze::load_env(); // Loads from .env in the working directory

    std::string db_url = blaze::env("DATABASE_URL");
    int port = blaze::env<int>("PORT", 8080);

    App app;
    app.listen(port);
}
```

---

## Production Recommendations

For a hardened production setup, we recommend the following pattern:

```cpp
int main() {
    App app;

    app.server_name("Blaze-Prod/1.0")
       .log_to("production.log")
       .log_level(LogLevel::INFO)
       .num_threads(std::thread::hardware_concurrency())
       .timeout(15)             // Fail fast in high load
       .max_body_size(1024*1024) // 1MB limit for standard APIs
       .enable_docs(false);     // Disable docs in public production

    app.listen_ssl(443, "cert.pem", "key.pem");
}
```
