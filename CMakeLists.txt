cmake_minimum_required(VERSION 3.15)

project(load_balancer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

add_library(lb_lib
    src/core/serve
    r.cpp

    proto/loadBalancer.pb.cc
    proto/loadBalancer.grpc.pb.cc
    proto/adminService.pb.cc
    proto/adminService.grpc.pb.cc
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