cmake_minimum_required(VERSION 3.20)

project(rosidl_runtime_cpython)

# Default to C11
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 11)
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)
find_package(rosidl_runtime_c REQUIRED)
find_package(rosidl_runtime_cpp REQUIRED)

# Prefer the unversioned system Python (matches rclpy's approach).
cmake_policy(SET CMP0094 NEW)
set(Python3_FIND_UNVERSIONED_NAMES FIRST)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

find_package(pybind11_vendor REQUIRED)
find_package(pybind11 REQUIRED)

# ---------------------------------------------------------------------------
# Pure Python package
# ---------------------------------------------------------------------------
ament_python_install_package(${PROJECT_NAME})

# ---------------------------------------------------------------------------
# C extension: rosidl_runtime_cpython._raw_buffer  (provides RawBuffer)
# ---------------------------------------------------------------------------
Python3_add_library(_raw_buffer MODULE src/raw_buffer.c)
target_include_directories(_raw_buffer PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  ${rosidl_runtime_c_INCLUDE_DIRS}
)
target_link_libraries(_raw_buffer PRIVATE
  rosidl_runtime_c::rosidl_runtime_c
)
set_target_properties(_raw_buffer PROPERTIES
  # Output into the rosidl_runtime_cpython/ subdirectory of the build space so that
  # PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR} resolves the submodule import
  # `from rosidl_runtime_cpython._raw_buffer import RawBuffer` correctly.
  LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_BINARY_DIR}/rosidl_runtime_cpython>
)

install(TARGETS _raw_buffer
  DESTINATION "${PYTHON_INSTALL_DIR}/rosidl_runtime_cpython"
)

# ---------------------------------------------------------------------------
# C++ extension: rosidl_runtime_cpython._raw_buffer_cpp  (pybind11 helpers)
# ---------------------------------------------------------------------------
pybind11_add_module(_raw_buffer_cpp src/raw_buffer_cpp.cpp)
target_include_directories(_raw_buffer_cpp PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  ${rosidl_runtime_c_INCLUDE_DIRS}
)
target_link_libraries(_raw_buffer_cpp PRIVATE
  rosidl_runtime_c::rosidl_runtime_c
)
set_target_properties(_raw_buffer_cpp PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_BINARY_DIR}/rosidl_runtime_cpython>
)
install(TARGETS _raw_buffer_cpp
  DESTINATION "${PYTHON_INSTALL_DIR}/rosidl_runtime_cpython"
)

# ---------------------------------------------------------------------------
# Core shared library: shared binding infrastructure (copy counter, etc.)
# consumed by both the _primitives extension and the generated message
# binding libraries. A regular (non-extension) library so the symbol is
# exported and shared across DSOs.
# ---------------------------------------------------------------------------
add_library(rosidl_runtime_cpython_core SHARED src/core.cpp)
target_include_directories(rosidl_runtime_cpython_core PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
  "$<INSTALL_INTERFACE:include>"
  ${rosidl_runtime_c_INCLUDE_DIRS}
)
target_link_libraries(rosidl_runtime_cpython_core PUBLIC
  rosidl_runtime_cpp::rosidl_runtime_cpp
  Python3::Python
)
install(TARGETS rosidl_runtime_cpython_core
  EXPORT rosidl_runtime_cpython_core_TARGETS
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
ament_export_targets(rosidl_runtime_cpython_core_TARGETS HAS_LIBRARY_TARGET)

# ---------------------------------------------------------------------------
# C++ extension: rosidl_runtime_cpython._primitives  (native typed wrappers)
# ---------------------------------------------------------------------------
pybind11_add_module(_primitives
  src/primitives.cpp
  src/scalar.cpp
  src/string.cpp
  src/sequence.cpp
  src/array.cpp
)
target_include_directories(_primitives PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  ${rosidl_runtime_c_INCLUDE_DIRS}
)
target_link_libraries(_primitives PRIVATE
  rosidl_runtime_cpp::rosidl_runtime_cpp
  rosidl_runtime_cpython_core
)
set_target_properties(_primitives PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_BINARY_DIR}/rosidl_runtime_cpython>
)
install(TARGETS _primitives
  DESTINATION "${PYTHON_INSTALL_DIR}/rosidl_runtime_cpython"
)

# ---------------------------------------------------------------------------
# Exported build interface
# ---------------------------------------------------------------------------
# Downstream C/C++ packages (e.g. rosidl_typesupport_xcdr_cpython) consume
# the C API declared in include/rosidl_runtime_cpython/raw_buffer.h without
# linking against the _raw_buffer Python module (the C API is resolved at
# runtime via PyCapsule_Import), so exporting the include directory is
# sufficient.
install(
  DIRECTORY include/
  DESTINATION include
)
# The wrapper headers in src/ are the shared binding infrastructure that
# generated message bindings (rosidl_generator_cpython) include; install them
# under include/rosidl_runtime_cpython/ so generated code can use the
# installed path.
install(
  DIRECTORY src/
  DESTINATION include/rosidl_runtime_cpython
  FILES_MATCHING PATTERN "*.hpp"
)
ament_export_include_directories(include)

# ---------------------------------------------------------------------------
# Testing
# ---------------------------------------------------------------------------
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  # Test-only extension: simulated experimental messages proving the
  # MessageHandleBase pattern (Phase 1B). Built and installed under
  # BUILD_TESTING so the pytest suite can import it.
  pybind11_add_module(_simulated
    test/rosidl_runtime_cpython/simulated_message.cpp
  )
  target_include_directories(_simulated PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    ${rosidl_runtime_c_INCLUDE_DIRS}
  )
  target_link_libraries(_simulated PRIVATE
    rosidl_runtime_cpp::rosidl_runtime_cpp
    rosidl_runtime_cpython_core
  )
  set_target_properties(_simulated PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_BINARY_DIR}/rosidl_runtime_cpython>
  )
  install(TARGETS _simulated
    DESTINATION "${PYTHON_INSTALL_DIR}/rosidl_runtime_cpython"
  )

  find_package(ament_cmake_pytest REQUIRED)
  set(_pytest_tests
    test/rosidl_runtime_cpython/test_primitives_string.py
    test/rosidl_runtime_cpython/test_primitives_sequence.py
    test/rosidl_runtime_cpython/test_primitives_reference.py
    test/rosidl_runtime_cpython/test_message_handle.py
  )
  foreach(_test_path ${_pytest_tests})
    get_filename_component(_test_name ${_test_path} NAME_WE)
    ament_add_pytest_test(${_test_name} ${_test_path}
      APPEND_ENV PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}
      TIMEOUT 60
    )
  endforeach()
endif()

ament_package()
