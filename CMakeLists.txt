cmake_minimum_required(VERSION 3.21)
project(hfsm VERSION 0.1.0 LANGUAGES CXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(HFSM_DEFAULT_BUILD_TARGETS ${PROJECT_IS_TOP_LEVEL})
option(HFSM_BUILD_SAMPLES "Build hfsm sample executables" ${HFSM_DEFAULT_BUILD_TARGETS})
option(HFSM_BUILD_TESTS "Build hfsm unit tests" ${HFSM_DEFAULT_BUILD_TARGETS})

add_library(hfsm INTERFACE)
add_library(hfsm::hfsm ALIAS hfsm)
target_compile_features(hfsm INTERFACE cxx_std_20)
target_include_directories(hfsm INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

if(HFSM_BUILD_SAMPLES)
  add_executable(hfsm-sample-basic
    samples/basic.cpp
  )
  target_link_libraries(hfsm-sample-basic PRIVATE hfsm::hfsm)

  add_executable(hfsm-sample-guard
    samples/guard.cpp
  )
  target_link_libraries(hfsm-sample-guard PRIVATE hfsm::hfsm)
endif()

if(HFSM_BUILD_TESTS)
  include(CTest)
  if(BUILD_TESTING)
    add_executable(hfsm-tests
      tests/test_hfsm.cpp
    )
    target_link_libraries(hfsm-tests PRIVATE hfsm::hfsm)
    add_test(NAME hfsm-tests COMMAND hfsm-tests)
  endif()
endif()

install(TARGETS hfsm
  EXPORT hfsmTargets
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT hfsmTargets
  FILE hfsmTargets.cmake
  NAMESPACE hfsm::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hfsm
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/hfsmConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/hfsmConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hfsm
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/hfsmConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/hfsmConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/hfsmConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hfsm
)
