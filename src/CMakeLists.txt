add_library(pecco_lib STATIC)

target_sources(pecco_lib
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lexer.cpp
)

target_compile_features(pecco_lib PUBLIC cxx_std_20)

target_include_directories(pecco_lib PUBLIC
  ${CMAKE_SOURCE_DIR}/include
  ${LLVM_INCLUDE_DIRS}
)

target_compile_definitions(pecco_lib PUBLIC ${LLVM_DEFINITIONS})
