cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(SkipMySong VERSION 0.1.0 LANGUAGES CXX)

set(_cxx_flags 
    CMAKE_CXX_FLAGS
    CMAKE_CXX_FLAGS_DEBUG
    CMAKE_CXX_FLAGS_RELEASE
    CMAKE_CXX_FLAGS_MINSIZEREL
    CMAKE_CXX_FLAGS_RELWITHDEBINFO
)
foreach(_cxx_flag ${_cxx_flags})
  string(REGEX REPLACE "/MDd?" "" ${_cxx_flag} "${${_cxx_flag}}")
endforeach()

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

find_package(wxWidgets CONFIG REQUIRED)
find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(cppwinrt CONFIG REQUIRED)

add_subdirectory(src)
