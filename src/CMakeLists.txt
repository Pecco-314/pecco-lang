add_library(pecco_lib STATIC)

target_sources(pecco_lib
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ast.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lexer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/symbol_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scope.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/symbol_table_builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operator_resolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scope_checker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type_checker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/codegen.cpp
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

llvm_map_components_to_libnames(llvm_libs support core irreader
  X86AsmParser X86Desc X86Info X86CodeGen
  MC MCParser Target Analysis Passes TransformUtils ScalarOpts InstCombine)

target_link_libraries(pecco_lib PUBLIC ${llvm_libs})

# plc executable
add_executable(plc driver.cpp)

target_link_libraries(plc
  PRIVATE
    pecco_lib
)

target_compile_features(plc PRIVATE cxx_std_20)
