# Copyright 2026 Ekumen Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

find_package(rosidl_cmake REQUIRED)
find_package(rosidl_runtime_c REQUIRED)
find_package(rosidl_typesupport_interface REQUIRED)
find_package(rosidl_typesupport_introspection_c REQUIRED)
find_package(rosidl_typesupport_introspection_cpython REQUIRED)

# Find python before pybind11 (rclpy pattern): the generated TUs use
# pybind11 to implement the MessageMember function pointers.
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
find_package(pybind11_vendor REQUIRED)
find_package(pybind11 REQUIRED)

set(_output_path
  "${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_introspection_cpython/${PROJECT_NAME}")

# Create a list of files that will be generated from each IDL file.
# Only experimental entries are generated: the introspection descriptors
# describe the experimental Python message classes (containers from
# rosidl_runtime_cpython).  Standard messages keep resolving through the C
# typesupport.
set(_generated_sources "")
foreach(_abs_idl_file ${rosidl_generate_interfaces_ABS_IDL_FILES})
  get_filename_component(_parent_folder "${_abs_idl_file}" DIRECTORY)
  get_filename_component(_parent_folder "${_parent_folder}" NAME)
  get_filename_component(_idl_name "${_abs_idl_file}" NAME_WE)
  # Turn idl name into file names
  string_camel_case_to_lower_case_underscore("${_idl_name}" _header_name)
  list(APPEND _generated_sources
    "${_output_path}/${_parent_folder}/experimental/detail/${_header_name}__type_support.cpp"
  )
endforeach()

# Create a list of IDL files from other packages that this generator should depend on
set(_dependency_files "")
set(_dependencies "")
foreach(_pkg_name ${rosidl_generate_interfaces_DEPENDENCY_PACKAGE_NAMES})
  foreach(_idl_file ${${_pkg_name}_IDL_FILES})
    set(_abs_idl_file "${${_pkg_name}_DIR}/../${_idl_file}")
    normalize_path(_abs_idl_file "${_abs_idl_file}")
    list(APPEND _dependency_files "${_abs_idl_file}")
    list(APPEND _dependencies "${_pkg_name}:${_abs_idl_file}")
  endforeach()
endforeach()

# Create a list of templates and source files this generator uses, and check that they exist
set(target_dependencies
  "${rosidl_typesupport_introspection_cpython_BIN}"
  ${rosidl_typesupport_introspection_cpython_GENERATOR_FILES}
  "${rosidl_typesupport_introspection_cpython_TEMPLATE_DIR}/idl__experimental_type_support.cpp.em"
  "${rosidl_typesupport_introspection_cpython_TEMPLATE_DIR}/msg__type_support.cpp.em"
  ${rosidl_generate_interfaces_ABS_IDL_FILES}
  ${_dependency_files})
foreach(dep ${target_dependencies})
  if(NOT EXISTS "${dep}")
    message(FATAL_ERROR "Target dependency '${dep}' does not exist")
  endif()
endforeach()

# Write all this to a file to work around command line length limitations on some platforms
set(generator_arguments_file
  "${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_introspection_cpython__arguments.json")
rosidl_write_generator_arguments(
  "${generator_arguments_file}"
  PACKAGE_NAME "${PROJECT_NAME}"
  IDL_TUPLES "${rosidl_generate_interfaces_IDL_TUPLES}"
  ROS_INTERFACE_DEPENDENCIES "${_dependencies}"
  OUTPUT_DIR "${_output_path}"
  TEMPLATE_DIR "${rosidl_typesupport_introspection_cpython_TEMPLATE_DIR}"
  TARGET_DEPENDENCIES ${target_dependencies}
)

cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0094 NEW)
set(Python3_FIND_UNVERSIONED_NAMES FIRST)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Add a command that invokes generator at build time
add_custom_command(
  OUTPUT ${_generated_sources}
  COMMAND Python3::Interpreter
  ARGS ${rosidl_typesupport_introspection_cpython_BIN}
  --generator-arguments-file "${generator_arguments_file}"
  DEPENDS ${target_dependencies}
  COMMENT "Generating CPython introspection type support"
  VERBATIM
)

set(_target_suffix "__rosidl_typesupport_introspection_cpython")

# Create a library that builds the generated files
add_library(${rosidl_generate_interfaces_TARGET}${_target_suffix}
  ${rosidl_typesupport_introspection_cpython_LIBRARY_TYPE} ${_generated_sources})

# Change output library name if asked to (this is the library the CPython
# dispatch map dlopens as "${pkg}__rosidl_typesupport_introspection_cpython").
if(rosidl_generate_interfaces_LIBRARY_NAME)
  set_target_properties(${rosidl_generate_interfaces_TARGET}${_target_suffix}
    PROPERTIES OUTPUT_NAME "${rosidl_generate_interfaces_LIBRARY_NAME}${_target_suffix}")
endif()

set_target_properties(${rosidl_generate_interfaces_TARGET}${_target_suffix}
  PROPERTIES
    DEFINE_SYMBOL "ROSIDL_TYPESUPPORT_INTROSPECTION_CPYTHON_BUILDING_DLL"
    CXX_STANDARD 17)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(${rosidl_generate_interfaces_TARGET}${_target_suffix}
    PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(${rosidl_generate_interfaces_TARGET}${_target_suffix} PUBLIC
  rosidl_runtime_c::rosidl_runtime_c
  rosidl_typesupport_interface::rosidl_typesupport_interface
  rosidl_typesupport_introspection_c::rosidl_typesupport_introspection_c
  rosidl_typesupport_introspection_cpython::rosidl_typesupport_introspection_cpython)
# pybind11 and Python3 are build-time details of the generated TUs: keep them
# out of the exported interface so consumers do not need to resolve them.
target_link_libraries(${rosidl_generate_interfaces_TARGET}${_target_suffix} PRIVATE
  pybind11::pybind11
  Python3::Python)

# Depend on dependencies
foreach(_pkg_name ${rosidl_generate_interfaces_DEPENDENCY_PACKAGE_NAMES})
  target_link_libraries(${rosidl_generate_interfaces_TARGET}${_target_suffix} PUBLIC
    ${${_pkg_name}_TARGETS${_target_suffix}})
endforeach()

# Make top level generation target depend on this library
add_dependencies(
  ${rosidl_generate_interfaces_TARGET}
  ${rosidl_generate_interfaces_TARGET}${_target_suffix}
)

if(NOT rosidl_generate_interfaces_SKIP_INSTALL)
  install(
    TARGETS ${rosidl_generate_interfaces_TARGET}${_target_suffix}
    EXPORT ${rosidl_generate_interfaces_TARGET}${_target_suffix}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )

  # Export old-style CMake variables
  ament_export_libraries(${rosidl_generate_interfaces_TARGET}${_target_suffix})

  # Export modern CMake targets
  ament_export_targets(${rosidl_generate_interfaces_TARGET}${_target_suffix})
  rosidl_export_typesupport_targets(${_target_suffix}
    ${rosidl_generate_interfaces_TARGET}${_target_suffix})

  ament_export_dependencies(
    "rosidl_runtime_c"
    "rosidl_typesupport_interface"
    "rosidl_typesupport_introspection_c"
    # Consumers linking the generated introspection library must be able to
    # resolve the rosidl_typesupport_introspection_cpython target in its
    # interface link libraries.
    "rosidl_typesupport_introspection_cpython")
endif()

if(BUILD_TESTING AND rosidl_generate_interfaces_ADD_LINTER_TESTS)
  find_package(ament_cmake_uncrustify REQUIRED)
  ament_uncrustify(
    TESTNAME "uncrustify_rosidl_typesupport_introspection_cpython"
    # the generated code might contain longer lines for templated types
    # a value of zero tells uncrustify to ignore line lengths
    MAX_LINE_LENGTH 0
    "${_output_path}")
endif()
