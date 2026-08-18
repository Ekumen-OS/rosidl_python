cmake_minimum_required(VERSION 3.20)
project(rosidl_typesupport_introspection_cpython)

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)
find_package(ament_cmake_ros REQUIRED)
find_package(rosidl_cmake REQUIRED)
find_package(rosidl_runtime_c REQUIRED)
find_package(rosidl_typesupport_interface REQUIRED)
find_package(rosidl_typesupport_introspection_c REQUIRED)

# ---------------------------------------------------------------------------
# Base runtime library (identifier only; the pybind11 helpers live in a header
# and are compiled into each generated per-package library).
# ---------------------------------------------------------------------------
add_library(${PROJECT_NAME}
  src/identifier.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    rosidl_runtime_c::rosidl_runtime_c
    rosidl_typesupport_interface::rosidl_typesupport_interface
    rosidl_typesupport_introspection_c::rosidl_typesupport_introspection_c)

target_compile_definitions(${PROJECT_NAME}
  PRIVATE "ROSIDL_TYPESUPPORT_INTROSPECTION_CPYTHON_BUILDING_DLL"
)

# Install library
install(
  TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# Install headers
install(
  DIRECTORY include/
  DESTINATION include/${PROJECT_NAME}
)

# Install Python package (the code generator)
ament_python_install_package(${PROJECT_NAME})

# Set Python install directory for extras file
if(NOT DEFINED PYTHON_INSTALL_DIR)
  set(PYTHON_INSTALL_DIR "lib/python3/dist-packages")
endif()

# Install generator script
install(
  PROGRAMS bin/rosidl_typesupport_introspection_cpython
  DESTINATION lib/rosidl_typesupport_introspection_cpython
)

# Install resource files (templates)
install(
  DIRECTORY resource
  DESTINATION share/${PROJECT_NAME}
)

# Install CMake files
install(
  DIRECTORY cmake
  DESTINATION share/${PROJECT_NAME}
)

ament_export_dependencies(
  rosidl_runtime_c
  rosidl_typesupport_interface
  rosidl_typesupport_introspection_c
)

ament_export_include_directories(include/${PROJECT_NAME})
ament_export_libraries(${PROJECT_NAME})
ament_export_targets(${PROJECT_NAME})

# Register this typesupport implementation with the CPython dispatch layer so
# middleware can resolve it from the CPython dispatch handle's type_support_map
# (same resource as rosidl_typesupport_xcdr_cpython).
ament_index_register_resource("rosidl_typesupport_cpython")

# Register as experimental-capable (supports experimental message namespaces)
ament_index_register_resource("available_experimental_typesupports")

if(BUILD_SHARED_LIBS)
  set(${PROJECT_NAME}_LIBRARY_TYPE "SHARED")
else()
  set(${PROJECT_NAME}_LIBRARY_TYPE "STATIC")
endif()

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_pytest REQUIRED)
  ament_add_pytest_test(
    test_introspection_cpython test/test_introspection_cpython.py)
endif()

ament_package(
  CONFIG_EXTRAS "rosidl_typesupport_introspection_cpython-extras.cmake.in"
)
