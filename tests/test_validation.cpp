#include <catch2/catch_test_macros.hpp>
#include <blaze/util/validation.h>
#include <blaze/exceptions.h>

using namespace blaze;

TEST_CASE("Validation: Core Helpers", "[validation]") {
    SECTION("required") {
        CHECK_NOTHROW(v::required("content"));
        CHECK_THROWS_AS(v::required(""), BadRequest);
    }

    SECTION("email") {
        CHECK_NOTHROW(v::email("test@example.com"));
        CHECK_NOTHROW(v::email("user.name+tag@domain.co.uk"));
        CHECK_THROWS_AS(v::email("invalid-email"), BadRequest);
        CHECK_THROWS_AS(v::email("@no-user.com"), BadRequest);
        CHECK_THROWS_AS(v::email("no-domain@"), BadRequest);
    }

    SECTION("min_len") {
        CHECK_NOTHROW(v::min_len("12345", 5));
        CHECK_NOTHROW(v::min_len("123456", 5));
        CHECK_THROWS_AS(v::min_len("1234", 5), BadRequest);
    }

    SECTION("max_len") {
        CHECK_NOTHROW(v::max_len("12345", 5));
        CHECK_NOTHROW(v::max_len("123", 5));
        CHECK_THROWS_AS(v::max_len("123456", 5), BadRequest);
    }

    SECTION("range") {
        CHECK_NOTHROW(v::range(10, 1, 10));
        CHECK_NOTHROW(v::range(5, 1, 10));
        CHECK_NOTHROW(v::range(1, 1, 10));
        CHECK_THROWS_AS(v::range(0, 1, 10), BadRequest);
        CHECK_THROWS_AS(v::range(11, 1, 10), BadRequest);
    }

    SECTION("matches") {
        std::regex re("^A.*Z$");
        CHECK_NOTHROW(v::matches("ABCZ", re));
        CHECK_THROWS_AS(v::matches("ABC", re), BadRequest);
    }

    SECTION("is_true") {
        CHECK_NOTHROW(v::is_true(true));
        CHECK_THROWS_AS(v::is_true(false), BadRequest);
    }
}
