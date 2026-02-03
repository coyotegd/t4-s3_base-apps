# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Point to the base components from the external submodule
set(EXTRA_COMPONENT_DIRS "external/hal_bsp/components" "external/hal_bsp/managed_components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(t4-s3_base-apps)
