set(RTB_PROTO_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/rtb.proto"
)

protobuf_generate_cpp(RTB_PROTO_SRCS RTB_PROTO_HDRS ${RTB_PROTO_FILES})

add_library(rtb_proto STATIC
    ${RTB_PROTO_SRCS}
    ${RTB_PROTO_HDRS}
)

target_include_directories(
    rtb_proto
    PUBLIC
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(
    rtb_proto
    PUBLIC
        protobuf::libprotobuf
)
