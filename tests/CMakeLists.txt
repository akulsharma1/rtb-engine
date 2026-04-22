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

add_executable(normalize_test normalize_test.cc)

target_link_libraries(
    normalize_test
    PRIVATE
        rtb_engine_core
)

add_test(NAME normalize_test COMMAND normalize_test)

add_executable(response_builder_test response_builder_test.cc)

target_link_libraries(
    response_builder_test
    PRIVATE
        rtb_engine_core
)

add_test(NAME response_builder_test COMMAND response_builder_test)

add_executable(handle_request_test handle_request_test.cc)

target_link_libraries(
    handle_request_test
    PRIVATE
        rtb_engine_core
)

add_test(NAME handle_request_test COMMAND handle_request_test)

add_executable(decision_test decision_test.cc)

target_link_libraries(
    decision_test
    PRIVATE
        rtb_engine_core
)

add_test(NAME decision_test COMMAND decision_test)

add_executable(campaign_store_test campaign_store_test.cc)

target_link_libraries(
    campaign_store_test
    PRIVATE
        rtb_engine_core
)

add_test(NAME campaign_store_test COMMAND campaign_store_test)

add_executable(retrieve_test retrieve_test.cc)

target_link_libraries(
    retrieve_test
    PRIVATE
        rtb_engine_core
)

add_test(NAME retrieve_test COMMAND retrieve_test)
