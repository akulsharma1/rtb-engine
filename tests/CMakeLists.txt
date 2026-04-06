add_executable(engine_types_smoke_test engine_types_smoke_test.cc)

target_link_libraries(
    engine_types_smoke_test
    PRIVATE
        rtb_engine_types
)

add_test(NAME engine_types_smoke_test COMMAND engine_types_smoke_test)
