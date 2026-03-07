#include <catch2/catch_test_macros.hpp>
#include <blaze/util/schema.h>
#include <blaze/model.h>

using namespace blaze;

struct SchemaTestModel {
    int id;
    std::string name;
    double score;
    bool is_valid;
};
BLAZE_MODEL(SchemaTestModel, id, name, score, is_valid)

TEST_CASE("Schema: Table Generation", "[schema]") {
    SECTION("Generate CREATE TABLE for SchemaTestModel") {
        std::string sql = schema::generate_create_table<SchemaTestModel>();
        
        CHECK(sql.find("CREATE TABLE IF NOT EXISTS \"schema_test_models\"") != std::string::npos);
        CHECK(sql.find("\"id\" INTEGER PRIMARY KEY GENERATED ALWAYS AS IDENTITY") != std::string::npos);
        CHECK(sql.find("\"name\" TEXT") != std::string::npos);
        CHECK(sql.find("\"score\" DOUBLE PRECISION") != std::string::npos);
        CHECK(sql.find("\"is_valid\" BOOLEAN") != std::string::npos);
    }
}
