set(US4_TESTS
  test_hardware
  test_mode
  test_backend
  test_adapter
  test_runtime
  test_hash
  test_agents
  test_virality
)

foreach(t ${US4_TESTS})
  add_executable(${t} ${t}.cpp)
  target_link_libraries(${t} PRIVATE us4_runtime)
  target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME ${t} COMMAND ${t})
endforeach()
