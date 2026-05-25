add_library(us4_runtime
  src/hardware.cpp
  src/mode.cpp
  src/backend.cpp
  src/backend_cpu.cpp
  src/adapter_dense.cpp
  src/adapter_registry.cpp
  src/tokenizer.cpp
  src/runtime.cpp
  src/hash.cpp
  src/agents.cpp
  src/virality.cpp
)

target_include_directories(us4_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(us4_runtime PUBLIC cxx_std_20)

find_package(Threads REQUIRED)
target_link_libraries(us4_runtime PUBLIC Threads::Threads)

# Apple: prefer the Accelerate framework for the NEON/BLAS backend.
if(APPLE)
  find_library(ACCELERATE_FRAMEWORK Accelerate)
  if(ACCELERATE_FRAMEWORK)
    target_link_libraries(us4_runtime PRIVATE ${ACCELERATE_FRAMEWORK})
    target_compile_definitions(us4_runtime PRIVATE US4_HAVE_ACCELERATE=1)
  endif()
endif()
