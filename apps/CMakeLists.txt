add_executable(us4-cli main.cpp)
target_link_libraries(us4-cli PRIVATE us4_runtime)
set_target_properties(us4-cli PROPERTIES OUTPUT_NAME us4-cli)
