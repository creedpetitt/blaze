include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(blaze_tests 
    test_main.cpp
    test_router.cpp
    test_di.cpp
    test_json.cpp
    test_model.cpp
    test_server.cpp
    test_crypto.cpp
    test_environment.cpp
    test_middleware.cpp
    test_websocket.cpp
    test_timeout.cpp
    test_injection.cpp
    test_app.cpp
    test_client_smart.cpp
    test_response.cpp
    test_static_files.cpp
    test_request.cpp
    test_multipart.cpp
    test_request_di.cpp
    test_snake_case.cpp
    test_validation.cpp
    test_schema.cpp
)


target_link_libraries(blaze_tests 
    PRIVATE 
    blaze::core
    Catch2::Catch2WithMain
)

# If we had DB tests
# target_link_libraries(blaze_tests PRIVATE blaze::mysql blaze::postgres)

include(Catch)
catch_discover_tests(blaze_tests)

add_subdirectory(integration_app)