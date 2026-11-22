cmake_minimum_required(VERSION 3.20)
project(rosidl_runtime_cpython)

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)
find_package(rosidl_runtime_c REQUIRED)

ament_python_install_package(${PROJECT_NAME})
install(DIRECTORY include/ DESTINATION include)
ament_export_include_directories(include)
ament_export_dependencies(rosidl_runtime_c)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  find_package(ament_cmake_pytest REQUIRED)
  find_package(ament_lint_auto REQUIRED)
  find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
  find_package(pybind11 REQUIRED)
  ament_lint_auto_find_test_dependencies()

  ament_add_gtest(test_borrowed_view test/test_borrowed_view.cpp)
  target_include_directories(test_borrowed_view PRIVATE include)

  pybind11_add_module(_test_borrowed_view test/test_borrowed_view_module.cpp)
  target_include_directories(_test_borrowed_view PRIVATE include)
  ament_add_pytest_test(test_python_borrowed_view test/test_borrowed_view.py
    APPEND_ENV PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR})
endif()

ament_package()
