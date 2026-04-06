add_library(rtb_engine_types INTERFACE)

target_include_directories(
    rtb_engine_types
    INTERFACE
        "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(
    rtb_engine_types
    INTERFACE
        rtb_project_warnings
)

add_executable(rtb_engine main.cc)

target_link_libraries(
    rtb_engine
    PRIVATE
        rtb_engine_types
        rtb_proto
)
