add_library(sinet STATIC
    src/protocol.cpp
    src/transport.cpp
    src/runtime.cpp
    src/registry.cpp
    src/receive.cpp
    src/send.cpp
    src/replication.cpp
    ${PROJECT_SOURCE_DIR}/client/net/client.cpp
    ${PROJECT_SOURCE_DIR}/server/net/server.cpp)
add_library(morgane::net ALIAS sinet)
target_include_directories(sinet PUBLIC include PRIVATE src)
target_link_libraries(sinet PUBLIC siecs::siecs sireflect::sireflect Threads::Threads)

if(BUILD_TESTING)
    add_executable(net_tests src/protocol_test.cpp)
    target_include_directories(net_tests PRIVATE src)
    target_link_libraries(net_tests PRIVATE sinet Catch2::Catch2WithMain)
    catch_discover_tests(net_tests)
endif()
