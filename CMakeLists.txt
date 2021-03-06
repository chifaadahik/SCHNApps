cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(SCHNApps
	LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")
set(CGAL_DONT_OVERRIDE_CMAKE_FLAGS TRUE CACHE BOOL "preserve cmake flags" FORCE)

#### Default build type
if(NOT CMAKE_BUILD_TYPE AND NOT MSVC)
	set(CMAKE_BUILD_TYPE "Release")
endif()

#### Numerical precision
option(SCHNAPPS_DOUBLE_PRECISION "Use double precision." ON)

#### Aligned Vector3
option(SCHNAPPS_ALIGNED_VEC3 "Use Eigen's AlignedVector3 by default." OFF)

#### SCHNAPPS PATH / SCHNAPPS_SOURCE_DIR
set(SCHNAPPS_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
set(SCHNAPPS_SOURCE_DIR ${SCHNAPPS_PATH})

set(SCHNAPPS_EXEC_NAME schnapps)

#### CGOGN_PATH 
set(CGOGN_BUILD_OR_INSTALL_PATH "${SCHNAPPS_PATH}/../CGoGN_2-build" CACHE PATH "CGoGN build or install dir")

if (EXISTS "${CGOGN_BUILD_OR_INSTALL_PATH}")
	set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CGOGN_BUILD_OR_INSTALL_PATH})
endif()

#### Build configuration
if (APPLE)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin/${SCHNAPPS_EXEC_NAME}.app/Contents/MacOS//lib)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
else()
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
endif()


if(MSVC)
	add_compile_options("/bigobj")
endif()

add_subdirectory(${SCHNAPPS_SOURCE_DIR}/schnapps)
