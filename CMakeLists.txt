cmake_minimum_required(VERSION 3.15)

project(load_balancer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

add_library(lb_lib
    src/core/server.cpp
    src/core/server_manager.cpp
    src/core/load_balancer.cpp
    src/core/process/process_factory.cpp
    src/core/process/linux_process.cpp
    src/api/admin_service.cpp
    src/strategies/round_robin.cpp

    proto/load_balancer.pb.cc
    proto/load_balancer.grpc.pb.cc
    proto/admin_service.pb.cc
    proto/admin_service.grpc.pb.cc
)

target_include_directories(lb_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/proto
)

target_link_libraries(lb_lib
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)

add_executable(load_balancer
    src/main.cpp
)

target_link_libraries(load_balancer
    PRIVATE
        lb_lib
)

add_executable(backend_server
    examples/backend_server_2.cpp
    proto/load_balancer.pb.cc
    proto/load_balancer.grpc.pb.cc
    proto/admin_service.pb.cc
    proto/admin_service.grpc.pb.cc
)

target_include_directories(backend_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/proto
)

target_link_libraries(backend_server
    PRIVATE
        protobuf::libprotobuf
        gRPC::grpc++
)

add_executable(client
    examples/client.cpp
    proto/load_balancer.pb.cc
    proto/load_balancer.grpc.pb.cc
)

target_include_directories(client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/proto
)

target_link_libraries(client
    PRIVATE
        protobuf::libprotobuf
        gRPC::grpc++
)

add_executable(admin_client
    examples/admin_client.cpp
    proto/admin_service.pb.cc
    proto/admin_service.grpc.pb.cc
)

target_include_directories(admin_client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/proto
)

target_link_libraries(admin_client
    PRIVATE
        protobuf::libprotobuf
        gRPC::grpc++
)

add_executable(health_checker
    src/monitoring/health_checker.cpp
    proto/admin_service.pb.cc
    proto/admin_service.grpc.pb.cc
)

target_include_directories(health_checker
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/proto
)

target_link_libraries(health_checker
    PRIVATE
        protobuf::libprotobuf
        gRPC::grpc++
)
