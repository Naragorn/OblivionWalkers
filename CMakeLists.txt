cmake_minimum_required(VERSION 3.20)
project(OblivionWalkers CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "Oblivion is 32-bit: configure with a 32-bit toolchain (vcvars32.bat, -A Win32, or cmake/toolchain-linux-clang-cl.cmake).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static runtime: the plugin carries no dependency on an installed VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_options(/W4 /permissive- /utf-8)
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)

# The pure decisions, process-free, so the tests run them on any machine.
add_library(logic INTERFACE)
target_include_directories(logic INTERFACE src)

add_library(plugin_entry STATIC src/PluginEntry.cpp)
target_include_directories(plugin_entry PUBLIC src)

add_library(rise STATIC src/Rise.cpp src/Replace.cpp src/Pose.cpp)
target_include_directories(rise PUBLIC src)

add_library(OblivionWalkers SHARED src/Plugin.cpp)
target_link_libraries(OblivionWalkers PRIVATE plugin_entry rise)

enable_testing()

add_executable(RiseLogicTest tests/RiseLogicTest.cpp)
target_link_libraries(RiseLogicTest PRIVATE logic)
add_test(NAME RiseLogicTest COMMAND RiseLogicTest)

add_executable(ReplaceLogicTest tests/ReplaceLogicTest.cpp)
target_link_libraries(ReplaceLogicTest PRIVATE logic)
add_test(NAME ReplaceLogicTest COMMAND ReplaceLogicTest)

add_executable(PoseLogicTest tests/PoseLogicTest.cpp)
target_link_libraries(PoseLogicTest PRIVATE logic)
add_test(NAME PoseLogicTest COMMAND PoseLogicTest)

add_executable(ApartLogicTest tests/ApartLogicTest.cpp)
target_link_libraries(ApartLogicTest PRIVATE logic)
add_test(NAME ApartLogicTest COMMAND ApartLogicTest)

add_executable(PluginEntryTest tests/PluginEntryTest.cpp)
target_link_libraries(PluginEntryTest PRIVATE plugin_entry)
add_test(NAME PluginEntryTest COMMAND PluginEntryTest)
