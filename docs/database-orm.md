# Database & ORM

Blaze provides a modular database layer that combines high-performance asynchronous drivers with a reflection-based ORM (Object-Relational Mapper).

---

## 1. Setup & Connection

### Installing Drivers
To keep your application as light as possible, database drivers are not included by default. You can add them using the Blaze CLI:

```bash
blaze add postgres  # Adds PostgreSQL driver
blaze add mysql     # Adds MySQL/MariaDB driver
```

### Establishing a Connection
The easiest way to connect is using the static `install()` helper in your `main()` function. This automatically creates a connection pool and registers it as the default `Database` service.

```cpp
#include <blaze/postgres.h> // or <blaze/mysql.h>

int main() {
    App app;

    // Connect to Postgres (Pool size defaults to 10)
    Postgres::install(app, "postgresql://user:pass@localhost/db");
    
    // Or Connect to MySQL
    // MySql::install(app, "mysql://user:pass@localhost/db");

    app.listen(8080);
}
```

### Advanced: Manual Registration
If you need to manage the pool lifetime yourself or connect to multiple databases, you can use the manual service registration API.

```cpp
// Open a pool with 10 connections (but don't register it yet)
auto pool = Postgres::open(app, "postgresql://user:pass@localhost/db", 10);

// Register it as the 'Database' service for the whole app
app.service(pool).as<Database>();
```

### Fault Tolerance (Circuit Breaker)
Database drivers in Blaze are protected by an automatic, high-concurrency **Circuit Breaker**. 
*   **Behavior**: If the database fails 5 times in a row, the breaker "trips".
*   **Concurrency**: Uses Acquire/Release memory barriers to ensure all worker threads immediately see the tripped state.
*   **Safety**: For the next 5 seconds, all database requests will immediately fail without attempting to connect. This prevents your application from overwhelming a struggling database or hanging threads on dead sockets.
*   **Recovery**: After 5 seconds, it allows one "probe" request. If successful, the breaker resets.

---

## 2. Defining Models (`BLAZE_MODEL`)

A "Model" is a simple C++ struct that represents a table in your database. By using the `BLAZE_MODEL` macro, you enable Blaze to automatically map database rows to your struct.

```cpp
struct Product {
    int id;
    std::string name;
    double price;
};

// This macro creates the "bridge" between C++ and SQL
BLAZE_MODEL(Product, id, name, price)
```

---

## 3. The Smart Repository

The **Repository** is your primary tool for interacting with the database. It provides a clean, type-safe API for CRUD (Create, Read, Update, Delete) operations.

### Automatic Naming
Blaze is smart about table names. If you have a struct named `UserAccount`, Blaze will look for a table named `user_accounts` (Snake Case + Pluralized).

### Basic Usage
You can request a repository for any model directly in your route handler.

```cpp
app.get("/products/:id", [](Path<int> id, Repository<Product> repo) -> Async<Product> {
    // 1. Fetch by Primary Key
    auto p = co_await repo.find(id);
    
    // 2. Return the object (Automatically converted to JSON)
    co_return p;
});
```

### Standard API:
*   **`find(id)`**: Fetches a single record. Throws `NotFound` if missing.
*   **`all()`**: Returns all records as a `std::vector<T>`.
*   **`save(model)`**: Inserts a new record. 
    *   *Note:* If the Primary Key is `0` (int) or empty (string), it is excluded from the query to allow the database to auto-increment/generate the ID.
*   **`update(model)`**: Updates an existing record (uses the first field as the ID).
*   **`remove(id)`**: Deletes a record.
*   **`count()`**: Returns the total number of rows.

---

## 4. The Fluent Query Builder

For more complex logic, use the `.query()` method to start a fluent chain.

```cpp
app.get("/search", [](Query<SearchParams> s, Repository<Product> repo) -> Async<Json> {
    auto results = co_await repo.query()
        .where("price", ">", s.min_price)
        .where("active", "=", true)
        .order_by("price", "DESC")
        .limit(10)
        .all();
        
    co_return Json(results);
});
```

Blaze automatically handles **SQL Injection** protection by using parameterized queries under the hood.

---

## 5. Raw Queries & Transactions

Sometimes you need to write raw SQL for performance, complex joins, or transactions. You can use the `Database` service directly.

```cpp
app.get("/stats", [](Database& db) -> Async<Json> {
    // Parameterized for safety!
    auto res = co_await db.query("SELECT count(*) as total FROM products WHERE price > $1", 100.0);
    
    // .as<int>() will throw InternalServerError if parsing fails
    int total = res[0]["total"].as<int>();
    co_return Json({{"high_value_items", total}});
});
```

### Transactions
Blaze provides a managed Transaction API that handles rollbacks automatically using RAII (Resource Acquisition Is Initialization) concepts.

```cpp
app.post("/transfer", [](Database& db) -> Async<Json> {
    // Start a transaction scope
    // "tx" is a special connection wrapper locked to this transaction
    co_await db.transaction([](Database& tx) -> Async<void> {
        
        // Use 'tx' for all queries in this block
        co_await tx.query("UPDATE accounts SET balance = balance - 100 WHERE id = 1");
        co_await tx.query("UPDATE accounts SET balance = balance + 100 WHERE id = 2");

        // If any exception is thrown here, the transaction automatically ROLLBACKs.
        // If the block completes successfully, it automatically COMMITs.
    });

    co_return Json({{"status", "success"}});
});
```

#### Auto-Injection in Transactions
For a cleaner syntax, you can ask for Repositories directly in the transaction lambda. Blaze will automatically bind them to the transaction's connection.

```cpp
app.post("/create-order", [](Database& db, Body<Order> order) -> Async<void> {
    
    // Inject Repositories directly!
    co_await db.transaction([&](Repository<Order> orders, Repository<Log> logs) -> Async<void> {
        
        // 'orders' and 'logs' are already connected to the transaction
        co_await orders.save(order);
        co_await logs.save({.msg = "Order created"});
        
    });
});
```

---

## 6. Handling Relationships

Blaze is intentionally designed as a **Micro-ORM**. It maps plain-old-data (POD) structs directly to tables without the heavy, hidden abstractions of traditional ORMs (like lazy-loading proxies or hidden `JOIN` generations). 

Because Blaze is fully asynchronous, magic relationships (e.g., `user.posts`) would either block the thread or require hidden database connections inside your models. Instead, Blaze encourages explicit, high-performance data fetching.

### Approach 1: Explicit Queries (Recommended)
The cleanest way to handle relationships (One-to-Many, Belongs-To) is by injecting the repositories you need and fetching the data explicitly.

```cpp
app.get("/users/:id/profile", [](Path<int> id, Repository<User> users, Repository<Post> posts) -> Async<Json> {
    // 1. Fetch User (Throws 404 if missing)
    User user = co_await users.find(id);
    
    // 2. Fetch related Posts using the fluent builder
    std::vector<Post> user_posts = co_await posts.query()
        .where("user_id", "=", id)
        .order_by("created_at", "DESC")
        .all();
    
    // 3. Combine into a JSON response
    Json response = user;
    response["posts"] = user_posts;
    
    co_return response;
});
```

### Approach 2: Raw SQL for Complex JOINs
If you need to fetch data from multiple tables at once for performance, bypass the Repository and use the `Database` service to write optimized `JOIN` statements. The raw results can be converted directly to JSON.

```cpp
app.get("/feed", [](Database& db) -> Async<Json> {
    // Write the exact, optimized SQL you want
    auto results = co_await db.query(R"(
        SELECT users.name, posts.title, posts.body 
        FROM users 
        JOIN posts ON users.id = posts.user_id 
        WHERE posts.published = true
        ORDER BY posts.created_at DESC 
        LIMIT 10
    )");
    
    // Returns a JSON array of objects automatically!
    co_return Json(results);
});
```

---

## 7. Schema Generation

Blaze can automatically generate the SQL required to create your tables by inspecting your `BLAZE_MODEL` structs. This is extremely useful for automating migrations or setting up fresh databases in integration tests.

### Usage
Include `<blaze/util/schema.h>` to access the generator.

```cpp
#include <blaze/util/schema.h>

struct User {
    int id;
    std::string email;
    bool active;
};
BLAZE_MODEL(User, id, email, active)

// Generate the SQL string
std::string sql = blaze::schema::generate_create_table<User>();

/*
Result:
CREATE TABLE IF NOT EXISTS "users" (
    "id" INTEGER PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    "email" TEXT,
    "active" BOOLEAN
);
*/
```

### Conventions
*   **Table Names**: Automatically pluralized and converted to `snake_case`.
*   **Primary Keys**: The first field in the struct is assumed to be the Primary Key.
*   **Identity**: Integer primary keys are automatically marked as `GENERATED ALWAYS AS IDENTITY` (PostgreSQL standard).
*   **Type Mapping**: C++ types (`int`, `std::string`, `bool`, `double`) are mapped to their closest SQL equivalents.
