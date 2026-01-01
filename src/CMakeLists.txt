add_library(pecco_lib STATIC)

target_sources(pecco_lib
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ast.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lexer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/symbol_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/declaration_collector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operator_resolver.cpp
)

target_compile_features(pecco_lib PUBLIC cxx_std_20)

target_compile_options(pecco_lib PUBLIC -fno-rtti)

target_include_directories(pecco_lib PUBLIC
  ${CMAKE_SOURCE_DIR}/include
  ${LLVM_INCLUDE_DIRS}
)

target_compile_definitions(pecco_lib PUBLIC ${LLVM_DEFINITIONS})

# Define STDLIB_DIR for prelude path
target_compile_definitions(pecco_lib PUBLIC
  STDLIB_DIR="${CMAKE_SOURCE_DIR}/stdlib"
)

# plc executable
add_executable(plc driver.cpp)

llvm_map_components_to_libnames(llvm_libs support)

target_link_libraries(plc
  PRIVATE
    pecco_lib
    ${llvm_libs}
)

target_compile_features(plc PRIVATE cxx_std_20)
