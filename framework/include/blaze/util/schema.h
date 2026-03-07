#ifndef BLAZE_UTIL_SCHEMA_H
#define BLAZE_UTIL_SCHEMA_H

#include <blaze/model.h>
#include <blaze/util/string.h>
#include <boost/describe.hpp>
#include <boost/core/demangle.hpp>
#include <string>
#include <sstream>
#include <vector>

namespace blaze::schema {

/**
 * @brief Internal helper to map C++ types to PostgreSQL-compatible SQL types.
 */
template<typename T>
inline std::string_view cpp_to_sql_type() {
    using PureT = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<PureT, int>) return "INTEGER";
    else if constexpr (std::is_same_v<PureT, long long> || std::is_same_v<PureT, int64_t>) return "BIGINT";
    else if constexpr (std::is_same_v<PureT, std::string>) return "TEXT";
    else if constexpr (std::is_same_v<PureT, double> || std::is_same_v<PureT, float>) return "DOUBLE PRECISION";
    else if constexpr (std::is_same_v<PureT, bool>) return "BOOLEAN";
    else return "TEXT"; // Fallback for unknown types
}

/**
 * @brief Generates a standard 'CREATE TABLE' SQL statement for a BLAZE_MODEL.
 * 
 * This utility uses reflection to inspect your struct fields and types. 
 * By default, it assumes the first field is the Primary Key.
 * 
 * @tparam T A struct defined with BLAZE_MODEL.
 * @return std::string The SQL statement (e.g., "CREATE TABLE IF NOT EXISTS users ...").
 */
template<typename T>
inline std::string generate_create_table() {
    std::string name = boost::core::demangle(typeid(T).name());
    
    // Use the same naming conventions as the Repository
    std::string table_name = util::pluralize(util::to_snake_case(name));

    std::ostringstream ss;
    ss << "CREATE TABLE IF NOT EXISTS \"" << table_name << "\" (\n";

    using Members = boost::describe::describe_members<T, boost::describe::mod_any_access>;
    bool first_field = true;
    bool pk_found = false;

    boost::mp11::mp_for_each<Members>([&](auto meta) {
        if (!first_field) ss << ",\n";
        
        std::string col_name = meta.name;
        using MemberType = typename std::remove_cvref_t<decltype(std::declval<T>().*(meta.pointer))>;
        
        ss << "    \"" << col_name << "\" " << cpp_to_sql_type<MemberType>();

        // Convention: First field is the primary key.
        // If it's an integer, we use 'GENERATED ALWAYS AS IDENTITY' (Modern Postgres Standard).
        if (!pk_found) {
            if constexpr (std::is_integral_v<MemberType>) {
                ss << " PRIMARY KEY GENERATED ALWAYS AS IDENTITY";
            } else {
                ss << " PRIMARY KEY";
            }
            pk_found = true;
        }

        first_field = false;
    });

    ss << "\n);";
    return ss.str();
}

} // namespace blaze::schema

#endif // BLAZE_UTIL_SCHEMA_H
