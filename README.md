<div align="center">
  <img src="docs/assets/logo.png" width = 500>
  <p><b>Blaze is a high-performance, asynchronous C++20 web framework designed for developer productivity and extreme scalability</b></p>


[![Blaze CI](https://github.com/Creed-Petitt/blaze/actions/workflows/ci.yml/badge.svg)](https://github.com/Creed-Petitt/blaze/actions/workflows/ci.yml)
![License](https://img.shields.io/badge/license-MIT-red)
![Platform](https://img.shields.io/badge/platform-linux%20%7C%20macos-red)
![Standard](https://img.shields.io/badge/c%2B%2B-20-red)

</div>

## Features

- **Asynchronous Coroutines**: High-concurrency powered by C++20 `co_await`.
- **Type-Safe Extraction**: Automatic injection of `Path<T>`, `Body<T>`, and `Query<T>`.
- **Auto-Wiring DI**: A robust IoC container that resolves dependencies automatically.
- **Reflection ORM**: Zero-boilerplate mapping between database, structs, and JSON.
- **Built-in Security**: Production-ready JWT, Rate Limiting, and CORS.
- **Automatic API Docs**: Swagger UI and OpenAPI 3.0 generated at compile-time.
- **High-Efficiency WebSockets**: Scalable real-time communication.
- **Hot Reload**: Automatic server restart on file changes.
- **Automatic Validation**: Integrity checks via `validate()` hooks.

## Installation

Install the **Blaze CLI** to scaffold and run projects with zero configuration.

```bash
// YOLO mode
curl -fsSL https://raw.githubusercontent.com/Creed-Petitt/blaze/main/install.sh | bash
```

> **Note:** The installer (`install.sh`) handles all system dependencies (CMake, OpenSSL, Drivers) automatically for you.
## Quick Start

1. **Initialize** your project:
   ```bash
   ~ blaze init my-api
     Creating project...
     Project ready!

   ~ cd my-api
   ```

2. **Write** your logic (e.g., in `src/main.cpp`):
   ```cpp
   #include <blaze/app.h>

   using namespace blaze;

   int main() {
       App app;

       app.get("/hello", [](Response& res) -> Async<void> {
           res.send("Hello from Blaze");
           co_return;
      });

       app.listen(8080);
   }
   ```

3. **Run** with hot-reload:
   ```bash
   ~ blaze run --watch
   ```

## Documentation

### Project Setup
- **[Getting Started](docs/getting-started.md)**: Installation and project setup.
- **[CLI Reference](docs/cli.md)**: Complete guide to all Blaze CLI commands.
- **[Configuration](docs/configuration.md)**: The App Builder API, logging, and environment variables.
- **[Architecture & Design](docs/architecture.md)**: Deep dive into the framework's core engine and design patterns.
- **[Testing & Security](docs/testing.md)**: CI/CD, sanitizers, and performance benchmarking.

### Core Framework
- **[Routing & Request Handling](docs/routing.md)**: Methods, parameters, and typed injection.
- **[Dependency Injection](docs/dependency-injection.md)**: Auto-wiring, lifetimes, and service registration.
- **[Database & ORM](docs/database-orm.md)**: Drivers, BLAZE_MODEL, and Repository pattern.
- **[HTTP Client](docs/http-client.md)**: Async requests, timeouts, and redirects.
- **[File Uploads](docs/file-uploads.md)**: Multipart forms and client-side uploads.
- **[Middleware & Security](docs/middleware.md)**: JWT Auth, CORS, and the middleware chain.
- **[WebSockets](docs/websockets.md)**: Real-time broadcasting and background tasks.
- **[Core Utilities](docs/utilities.md)**: Zero-allocation tools for strings, JWTs, and resilience.

## Requirements

Blaze is designed for modern Linux and macOS environments.

- **Compiler**: C++20 compliant (GCC 11+, Clang 13+, or MSVC 2022+).
- **Libraries**: OpenSSL, libpq (PostgreSQL), and libmariadb (MySQL).
- **Engine**: Boost 1.85+ (Automatically managed via CMake).

## License

Blaze is released under the [MIT License](LICENSE).
