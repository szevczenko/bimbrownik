# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(COMPONENTS main)
set(EXTRA_COMPONENT_DIRS hq_components)
set(PROJECT_VER "0.0.8")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bimbrownik)

idf_build_set_property(COMPILE_OPTIONS -DHTTP_URL="http://0.0.0.0:80" APPEND)
idf_build_set_property(COMPILE_OPTIONS -DHTTPS_URL="https://0.0.0.0:443" APPEND)
