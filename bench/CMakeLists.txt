add_executable(rtb_inproc_bench request_pipeline_bench.cc)

target_link_libraries(
    rtb_inproc_bench
    PRIVATE
        rtb_engine_core
)
