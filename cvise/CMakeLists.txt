## -*- mode: CMake -*-
##
## Copyright (c) 2012, 2013, 2014, 2015, 2016, 2018, 2019 The University of Utah
## All rights reserved.
##
## This file is distributed under the University of Illinois Open Source
## License.  See the file COPYING for details.

###############################################################################

cmake_minimum_required(VERSION 2.8.12)

project(cvise_python)

###############################################################################

# find_package(LLVM) is done by the topmost "CMakeLists.txt" file.

###############################################################################

# Check for the run-time prerequisites of C-Vise.  We only warn the user when
# these are not found at configure time.  Let the user install the dependencies
# later.
#
find_program(CLANG_FORMAT
  "clang-format${CMAKE_EXECUTABLE_SUFFIX}"
  PATHS "${LLVM_TOOLS_BINARY_DIR}"
  )
if(NOT CLANG_FORMAT)
  message(STATUS "`clang-format${CMAKE_EXECUTABLE_SUFFIX}' was not found")
  set(missing_required_runtime_prereq "yes")
endif()


if(NOT CLANG_FORMAT)
  message(STATUS "`clang-format${CMAKE_EXECUTABLE_SUFFIX}' was not found")
  message("You must install `clang-format' before running C-Vise.")
  set(CLANG_FORMAT "clang-format")
endif()

###############################################################################

# Generate file "cvise.py".
#
configure_file("${cvise_python_SOURCE_DIR}/cvise.py"
  "${cvise_python_BINARY_DIR}/cvise.py"
)

###############################################################################

# Copy the Python modules into the build tree so that we can run C-Vise there.
#
function(configure_one_file path)
  configure_file(
    "${cvise_python_SOURCE_DIR}/${path}"
    "${cvise_python_BINARY_DIR}/${path}"
    COPYONLY
  )
endfunction(configure_one_file)

set(SOURCE_FILES
  "__init__.py"
  "pass_groups/all.json"
  "pass_groups/opencl-120.json"
  "passes/__init__.py"
  "passes/abstract.py"
  "passes/balanced.py"
  "passes/blank.py"
  "passes/clang.py"
  "passes/clangbinarysearch.py"
  "passes/clex.py"
  "passes/comments.py"
  "passes/ifs.py"
  "passes/includeincludes.py"
  "passes/includes.py"
  "passes/indent.py"
  "passes/ints.py"
  "passes/line_markers.py"
  "passes/lines.py"
  "passes/peep.py"
  "passes/special.py"
  "passes/ternary.py"
  "passes/unifdef.py"
  "tests/__init__.py"
  "tests/testabstract.py"
  "tests/test_balanced.py"
  "tests/test_comments.py"
  "tests/test_ifs.py"
  "tests/test_ints.py"
  "tests/test_line_markers.py"
  "tests/test_nestedmatcher.py"
  "tests/test_peep.py"
  "tests/test_special.py"
  "tests/test_ternary.py"
  "utils/__init__.py"
  "utils/error.py"
  "utils/nestedmatcher.py"
  "utils/readkey.py"
  "utils/statistics.py"
  "utils/testing.py"
)

foreach(file IN LISTS SOURCE_FILES)
  configure_one_file(${file})
endforeach()

###############################################################################

install(DIRECTORY "${cvise_python_BINARY_DIR}/"
  DESTINATION "${CMAKE_INSTALL_PREFIX}/share/${cvise_PACKAGE}"
  FILES_MATCHING
  PATTERN "*.py"
  PATTERN "*.json"
  PATTERN "__pycache__" EXCLUDE
  PATTERN "CMakeFiles" EXCLUDE
)

###############################################################################

## End of file.
