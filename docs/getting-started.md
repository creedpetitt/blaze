# Getting Started

This guide walks you through building your first high-performance web application with Blaze. Blaze is designed to feel as productive as Python or Go while maintaining the extreme performance of C++20 coroutines.

---

## 1. Include the Framework

Starting with an empty `main.cpp` file, first include the core application header. This header provides the `App` class and the necessary primitives for routing.

```cpp
#include <blaze/app.h>

using namespace blaze;
```

---

## 2. Declare the App

Next, create a `main()` function and declare a `blaze::App` instance.

```cpp
int main() {
    App app;
}
```

The **App** class is the central nervous system of your application. It manages:
*   **The Event Loop**: Powered by `boost::asio`, handling thousands of connections.
*   **Dependency Injection**: Auto-filling your handlers with the services they need.
*   **The Router**: Mapping URLs to your C++ logic.

---

## 3. Adding Your First Route

Once you have your app, the next step is to define your endpoints. In Blaze, routes are defined using lambda expressions or function pointers that return an `Async<void>` (a coroutine).

```cpp
app.get("/", [](Response& res) -> Async<void> {
    res.send("Hello from Blaze!");
    co_return;
});
```

### Pro-Tip: Protected Routes
Need to check if a user is logged in? Use `req.is_authenticated()`. Blaze's identity system makes it easy to secure your API with just a few lines of code. Check the **Middleware & Security** guide for full details on JWT and Identity.

### What's happening here?
1.  **`app.get("/", ...)`**: We are registering a handler for HTTP `GET` requests at the root path (`/`).
2.  **`[](Response& res)`**: Blaze automatically "injects" a **Response** object into your handler. You use this to talk back to the client.
3.  **`-> Async<void>`**: This marks the function as a C++20 coroutine. This is why Blaze is so fast—it can "suspend" your function if it's waiting for I/O, freeing up the CPU for other users.
4.  **`res.send(...)`**: This tells Blaze to prepare a plain-text response.
5.  **`co_return`**: Since this is a coroutine, we use `co_return` instead of `return`.

### Production HTTPS (SSL)

In a production environment, you should always serve your API over HTTPS. Blaze makes this simple with `listen_ssl()`.

```cpp
int main() {
    App app;
    // ... routes ...

    // Provide the path to your certificate and private key
    app.listen_ssl(443, "cert.pem", "key.pem");
}
```

Blaze uses **OpenSSL** internally to handle the encryption. The framework automatically configures modern TLS defaults (TLS 1.2+) to ensure your data is secure during transit.

### Application Configuration

Blaze provides a fluent "Builder" API to configure your server's behavior. You should apply these settings before calling `listen()`.

```cpp
int main() {
    App app;

    app.server_name("MyBlazeApp/1.0")  // Custom 'Server' header
       .max_body_size(5 * 1024 * 1024) // Limit uploads to 5MB
       .timeout(60)                    // 60s request timeout
       .num_threads(8)                 // Specify thread pool size
       .enable_docs(false);            // Disable Swagger UI (/docs)

    app.listen(8080);
}
```

For a full list of settings and best practices, see the **[Configuration Guide](configuration.md)**.

---

### Logging

Blaze features an asynchronous, thread-safe logger that won't slow down your request handling. You can configure the destination and the detail level.

```cpp
int main() {
    App app;

    app.log_to("server.log")           // Redirect logs to a file
       .log_level(LogLevel::DEBUG);    // Set minimum level (DEBUG, INFO, WARN, ERROR)

    // Disable logging entirely
    // app.log_to("/dev/null");

    app.listen(8080);
}
```

By default, Blaze logs every incoming request with an `ACCESS` prefix, including the client's IP, method, path, status code, and response time.

---

## Structure & Scalability

For larger applications, you should move your routes into **Controllers**. A controller is just a class with a static `register_routes` method.

```cpp
class UserController {
public:
    static void register_routes(App& app) {
        app.get("/users", &UserController::list);
    }

    static Async<void> list(Response& res) {
        res.send("User list");
        co_return;
    }
};

int main() {
    App app;
    // Register multiple controllers at once
    app.register_controllers<UserController, AuthController>();
    app.listen(8080);
}
```

### Pro-Tip: Named Functions for Stability
While lambdas are great for simple routes, you should prefer **Named Functions** or **Controllers** for any logic involving `co_await`.

Modern C++ compilers (like GCC 11) perform complex state-machine transformations for coroutines. In very large projects, deeply nested lambdas with many `co_await` calls can occasionally trigger "Internal Compiler Errors" (ICE). Moving logic to named functions or class methods solves this completely and improves compile times.

---

## 4. Scaling & Stability

As your application grows, Blaze provides the tools to maintain a clean, stable codebase.

### Controller Pattern
Organize your API into logical blocks. A controller is a class where you group related logic.

```cpp
class ProductController {
public:
    static void register_routes(App& app) {
        auto products = app.group("/products");
        products->get("/", &ProductController::list);
        products->get("/:id", &ProductController::get_one);
    }

    static Async<void> list(Response& res) {
        // ... logic ...
        co_return;
    }
};
```

### The "Named Function" Rule
To ensure maximum stability and debuggability in production apps:
1.  **Use Lambdas** for simple, one-line responses or basic middleware.
2.  **Use Named Functions** for any handler that interacts with a Database, Filesystem, or External API.
3.  **Use Controllers** to group those named functions into resource-based modules (Users, Orders, etc.).

#### **Technical Note: Why `static`?**
When using controllers, your handler functions must be declared as `static`. 

In C++, non-static member functions require an **instance** of the class to be called (they have a hidden `this` pointer). By using `static`, you turn the method into a regular function pointer that doesn't depend on an object's state. This allows the Blaze Dependency Injection engine to call the function directly and inject exactly what you've requested in the arguments (like `Database&` or `Response&`) without needing to manage the lifecycle of the controller class itself.

---

## 4. Running the App

After defining your routes, instruct Blaze to start the server. This is done using the `listen()` method.

```cpp
app.listen(8080);
```

By default, `listen()` is **blocking**. It starts the internal thread pool and waits for incoming connections. If you need to run the app asynchronously, you can use `app.spawn()` for background tasks or run the event loop manually via `app.engine().run()`.

---

## Putting It All Together

Here is the complete, production-ready code for a Hello World API:

```cpp
#include <blaze/app.h>

using namespace blaze;

int main() {
    App app;

    // Define a basic route
    app.get("/", [](Response& res) -> Async<void> {
        res.send("Hello World");
        co_return;
    });

    // Run on port 8080
    app.listen(8080);
}
```

### Compiling and Running
If you used the Blaze CLI to initialize your project, simply run:

```bash
blaze run --watch
```

This command will compile your code, start the server, and automatically reload it whenever you save a change.

---

## IDE Support

Blaze is designed to work seamlessly with modern C++ IDEs like **CLion**, **VS Code**, and **Visual Studio**. 

Thanks to our partitioned global cache, you can switch between the `blaze` CLI and your IDE's "Run" button without any CMake cache conflicts. 

**Pro-Tips for IDEs:**
1.  **Code Completion**: Blaze's heavy use of reflection and headers ensures that "Go to Definition" and Parameter Hints work perfectly.
2.  **Debugging**: You can set breakpoints inside your route handlers and step directly into the Blaze framework source code.
3.  **Formatting**: We recommend using the provided `.clang-format` file in the project root to keep your code consistent with the framework.

> **Tip:** For advanced commands like Docker integration (`blaze docker`) or Code Generators (`blaze generate`), check out the **[CLI Reference](cli.md)**.

---

## Advanced Essentials

### Environment Variables
Most applications need configuration (API keys, ports). Blaze provides a built-in, type-safe environment manager.

```cpp
#include <blaze/environment.h>

int main() {
    // 1. Load the .env file
    blaze::load_env();

    // 2. Fetch variables with a default fallback
    int port = blaze::env<int>("PORT", 8080);
    
    // 3. Or fetch strictly (throws if missing)
    std::string key = blaze::env("SECRET_KEY");

    App app;
    app.listen(port);
}
```

### The Async HTTP Client
Need to talk to another API? Use the built-in `blaze::fetch`. It uses the same coroutine engine as the server, so it never blocks your threads.

```cpp
#include <blaze/client.h>

app.get("/proxy", []() -> Async<Json> {
    // This looks like synchronous code, but it's 100% non-blocking!
    auto res = co_await blaze::fetch("https://api.github.com/repos/Creed-Petitt/blaze");
    
    if (res.status == 200) {
        co_return res.body; // res.body is a blaze::Json object
    }
    
    throw InternalServerError("Upstream API failed");
});
```

For advanced usage (Timeouts, Redirects, File Uploads), check out the [HTTP Client Documentation](http-client.md).
