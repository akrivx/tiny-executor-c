cmake_minimum_required(VERSION 3.25)

project(texec VERSION 0.1.0 LANGUAGES C)

include(CheckCCompilerFlag)
include(GNUInstallDirs)

option(TEXEC_BUILD_EXAMPLES "Build texec examples" ON)

set(TEXEC_C_STANDARD 17)
if(MSVC)
  check_c_compiler_flag("/std:c23" TEXEC_MSVC_HAS_C23)
  if(TEXEC_MSVC_HAS_C23)
    set(TEXEC_C_STANDARD 23)
  endif()
endif()

add_library(texec
  src/default_allocator.c
  src/executor.c
  src/queue.c
  src/task_group.c
  src/task_handle.c
  src/thread_pool_executor.c
)

add_library(texec::texec ALIAS texec)

set_target_properties(texec PROPERTIES
  C_STANDARD ${TEXEC_C_STANDARD}
  C_STANDARD_REQUIRED YES
  C_EXTENSIONS NO
)

target_include_directories(texec
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(Threads REQUIRED)
target_link_libraries(texec PUBLIC Threads::Threads)

if(MSVC)
  target_compile_options(texec PRIVATE /W4 /permissive- /Zc:preprocessor /experimental:c11atomics)
else()
  target_compile_options(texec PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS texec
  EXPORT texecTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT texecTargets
  NAMESPACE texec::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/texec
)

if(TEXEC_BUILD_EXAMPLES)
  add_executable(texec_example
    examples/texec_example.c
  )
  target_link_libraries(texec_example PRIVATE texec)
  set_target_properties(texec_example PROPERTIES
    C_STANDARD ${TEXEC_C_STANDARD}
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
  )
  if(MSVC)
    target_compile_options(texec_example PRIVATE /experimental:c11atomics)
  endif()
endif()
