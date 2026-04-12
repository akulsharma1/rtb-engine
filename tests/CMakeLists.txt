add_executable(engine_types_smoke_test engine_types_smoke_test.cc)

target_link_libraries(
    engine_types_smoke_test
    PRIVATE
        rtb_engine_types
)

add_test(NAME engine_types_smoke_test COMMAND engine_types_smoke_test)

add_executable(parse_test parse_test.cc)

target_link_libraries(
    parse_test
    PRIVATE
        rtb_engine_core
)

add_test(NAME parse_test COMMAND parse_test)

add_executable(frame_test frame_test.cc)

target_link_libraries(
    frame_test
    PRIVATE
        rtb_engine_core
)

add_test(NAME frame_test COMMAND frame_test)
