# Routing & Request Handling

Routing is the process of mapping an incoming HTTP request to a specific piece of C++ logic. Blaze features a powerful, type-safe router that eliminates the boilerplate of manual request parsing.

---

## 1. Basic Routes

You register routes directly on the `App` instance using HTTP method names.

```cpp
app.get("/hello", [](Response& res) -> Async<void> {
    res.send("Hello!");
    co_return;
});

app.post("/submit", [](Request& req, Response& res) -> Async<void> {
    // Handle the request...
    co_return;
});
```

Blaze supports `get`, `post`, `put`, and `del` (for DELETE).

---

## 2. Magic Typed Injection

This is Blaze's most powerful feature. Instead of manually extracting data from `Request`, you can simply declare what you need as function arguments. Blaze will "auto-fill" them for you.

### Path Parameters (`Path<T>`)
Use `:id` syntax in your URL to capture variables. Blaze automatically converts them to your target C++ type.

```cpp
// Route: /users/:id/files/:file_id
app.get("/users/:id/files/:file_id", [](Path<int> uid, Path<int> fid) -> Async<Json> {
    // uid and fid act like normal integers!
    co_return Json({{"user_id", uid}, {"file_id", fid}});
});
```

### Request Body (`Body<T>`)
If you send JSON in the request body, Blaze can parse it directly into a C++ struct.

```cpp
struct User {
    std::string name;
    int age;
};
BLAZE_MODEL(User, name, age) // Enable reflection

app.post("/users", [](Body<User> user) -> Async<void> {
    // Access members directly
    std::cout << "Saving user: " << user.name << std::endl;
    co_return;
});
```

### Query Parameters (`Query<T>`)
Handle query strings like `?page=2&q=blaze` by mapping them to a struct.

```cpp
struct Search {
    std::string q;
    int page;
};
BLAZE_MODEL(Search, q, page)

app.get("/search", [](Query<Search> s) -> Async<Json> {
    co_return Json({{"searching_for", s.q}, {"page", s.page}});
});
```

### Requirements & Ergonomics

#### 1. Models are Mandatory
`Query<T>` and `Body<T>` require `T` to be a struct defined with `BLAZE_MODEL`. This is for **Static Extraction**—Blaze maps your fields at compile-time for zero-overhead performance and automatic Swagger documentation.

#### 2. Inheritance Ergonomics
`Path<T>` and `Body<T>` **inherit** from your type `T`. This is a core ergonomic feature of Blaze: it means you don't need to call `.value()` or `.get()` to access your data. These wrappers *are* your types, augmented with metadata for the router.

```cpp
app.get("/greet/:name", [](Path<std::string> name) -> Async<void> {
    // 'name' IS a std::string!
    std::cout << "Hello, " << name << " (length: " << name.length() << ")" << std::endl;
    co_return;
});
```

#### 3. Dynamic Escape Hatch (`req.json()`)
If you need to handle unpredictable JSON or don't want to define a model, use `req.json()` to get a dynamic `blaze::Json` object.

```cpp
app.post("/dynamic", [](Request& req, Response& res) -> Async<void> {
    auto body = req.json();
    std::string type = body["type"].as<std::string>();
    res.send("Handled dynamic type: " + type);
    co_return;
});
```

#### 4. Static vs. Dynamic JSON
Choosing the right tool for JSON handling is key to balancing performance and flexibility.

| Feature | `Body<T>` (Static) | `req.json()` (Dynamic) |
| :--- | :--- | :--- |
| **Performance** | **Ultra-Fast**: Mapped at compile-time. | **Standard**: Parsed into a map-like structure. |
| **Validation** | **Automatic**: Checks types and calls `validate()`. | **Manual**: You must check field existence/types. |
| **Documentation** | **Auto-Swagger**: Generates JSON Schemas. | **Generic**: Documented as an opaque object. |
| **Best For** | Public APIs, DB Models, Strict Contracts. | Webhooks, Prototyping, Heterogeneous data. |

---

## 3. Automatic Validation

If your model has a `validate()` method, Blaze will automatically call it before your handler runs. To make this easier, Blaze provides a set of declarative helpers in the `blaze::v` namespace.

```cpp
#include <blaze/util/validation.h>

struct Signup {
    std::string email;
    std::string password;
    int age;

    void validate() const {
        v::required(email, "Email");
        v::email(email);
        v::min_len(password, 8, "Password");
        v::range(age, 18, 120, "Age");
    }
};
BLAZE_MODEL(Signup, email, password, age)

app.post("/signup", [](Body<Signup> s) -> Async<void> {
    // If we get here, the data is GUARANTEED to be valid!
    co_return;
});
```

### Available Helpers (`blaze::v`)
| Helper | Description |
| :--- | :--- |
| `v::required(val, name)` | Throws if string is empty. |
| `v::email(val, name)` | Validates email format via regex. |
| `v::min_len(val, min, name)` | Validates minimum string length. |
| `v::max_len(val, max, name)` | Validates maximum string length. |
| `v::range(val, min, max, name)` | Validates numeric range (inclusive). |
| `v::matches(val, regex, name)` | Validates against a custom `std::regex`. |
| `v::is_true(val, name)` | Throws if boolean is false (useful for TOS). |

---

## 4. JSON Power Tools

While `Body<T>` is great for structs, sometimes you need to construct dynamic JSON or Arrays on the fly.

```cpp
// 1. JSON Arrays
// Returns: [1, 2, 3]
res.json(Json::array(1, 2, 3));

// 2. Mixed Types
// Returns: ["Error", 404, true]
res.json(Json::array("Error", 404, true));

// 3. Dynamic Construction
Json data;
data["status"] = "ok";
data["values"] = Json::array(10, 20);
res.json(data);
```

---

## 5. The Response Builder

Blaze uses a **Fluent Interface** for responses. This means you can chain methods together like a sentence.

```cpp
app.get("/custom", [](Response& res) -> Async<void> {
    res.status(201)
       .header("X-Custom", "Blaze")
       .json({{"status", "created"}});
    co_return;
});
```

### Semantic Shortcuts
Blaze provides semantic helper methods to make your code read like English.

| Method | Status Code | Use Case |
| :--- | :--- | :--- |
| **`.created(location)`** | `201` | Resource created. Optional `Location` header. |
| **`.accepted()`** | `202` | Request received, processing in background. |
| **`.no_content()`** | `204` | Action successful, no body to return. |
| **`.bad_request(msg)`** | `400` | Client sent invalid data (returns JSON error). |
| **`.unauthorized(msg)`** | `401` | Missing or invalid authentication token. |
| **`.forbidden(msg)`** | `403` | Authenticated, but permissions denied. |
| **`.not_found(msg)`** | `404` | Resource does not exist. |

```cpp
// Example: Creating a user
app.post("/users", [](Body<User> user, Repository<User> users) -> Async<void> {
    co_await users.save(user);
    res.created("/users/" + std::to_string(user.id));
    co_return;
});
```

### `send()` vs `json()`

*   **`.send(text)`**: Used for plain text, HTML, or raw bodies. It sends the string exactly as provided.
*   **`.json(data)`**: Used for `blaze::Json` or any struct with `BLAZE_MODEL`. It automatically sets the `Content-Type: application/json` header and serializes the data for you.

---

## 6. Route Groups

Organize your API into logical blocks or versions using `group()`. You can also **nest** groups for granular versioning.

```cpp
// 1. Create a top-level group
auto api = app.group("/api");

// 2. Create a nested group (Result: /api/v1)
auto v1 = api.group("/v1");

// 3. Register routes (Result: /api/v1/status)
v1.get("/status", [](Response& res) -> Async<void> {
    res.send("Online");
    co_return;
});
```

---

## 7. The Request Object

While magic injection is preferred, sometimes you need direct access to the `Request` object. It acts as a "Bucket" for all incoming data.

```cpp
app.get("/manual", [](Request& req, Response& res) -> Async<void> {
    // 1. Headers
    std::string_view ua = req.get_header("User-Agent");

    // 2. Cookies
    std::string session = req.cookie("session_id");

    // 3. User Identity (See Middleware & Security guide for more)
    if (req.is_authenticated()) {
        Json user = req.user();
        std::cout << "User ID: " << user["id"] << std::endl;
    }

    // 4. Manual DI Resolution
    auto db = req.resolve<Database>();

    res.send("Handled manually");
    co_return;
});
```

---

## 8. Automatic API Documentation (Swagger)

Blaze automatically generates an **OpenAPI 3.0** specification for your API by inspecting your route handlers and models at compile-time.

When you run your app, the following endpoints are available:
*   `GET /openapi.json`: The raw OpenAPI spec.
*   `GET /docs`: An interactive **Swagger UI** where you can test your endpoints.

Because Blaze uses reflection, your `Body<User>` or `Query<Search>` arguments are automatically documented with their correct JSON schemas—no manual annotations required.

---

## 9. Error Handling

Blaze uses exceptions to handle HTTP errors. If you throw an `HttpError` (or any of its children) inside a handler, Blaze will automatically catch it and return the correct status code and a JSON error message to the client.

```cpp
app.get("/secret", []() -> Async<void> {
    if (!authorized) {
        throw Unauthorized("Keep out!");
    }
    co_return;
});
```

### Common Exceptions:
*   **`BadRequest("msg")`**: Returns 400.
*   **`Unauthorized("msg")`**: Returns 401.
*   **`Forbidden("msg")`**: Returns 403.
*   **`NotFound("msg")`**: Returns 404.
*   **`InternalServerError("msg")`**: Returns 500.

Standard C++ JSON libraries can be verbose. Blaze includes a lightweight wrapper around `boost::json` to make your code "neat."

```cpp
// Creation
Json data = {{"id", 1}, {"name", "Blaze"}};

// Access (Type-safe)
std::string name = data["name"].as<std::string>();
int id = data["id"].as<int>(); // Throws BadRequest(400) if not a valid integer

// Serialization
std::string raw_json = data.dump();

// Safety
if (data.has("optional_field")) { ... }
```
