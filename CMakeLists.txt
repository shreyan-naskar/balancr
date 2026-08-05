cmake_minimum_required(VERSION 3.15)

project(load_balancer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

add_library(lb_lib
    src/core/server.cpp

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