cmake_minimum_required(VERSION 3.20)

# The tests on their own, built natively but 32-bit, for working on Linux:
# none of them needs Windows, and the DLL build (clang-cl against xwin's
# winsysroot) makes Windows executables that cannot run here.
#
#   cmake -S tests -B build-tests -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS=-m32
#   cmake --build build-tests && ctest --test-dir build-tests --output-on-failure

project(OblivionWalkersTests LANGUAGES CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "The tests must be built 32-bit (clang++ with -m32): the mirrored engine structures hold for that pointer size.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

foreach(name RiseLogicTest ReplaceLogicTest CategoryLogicTest PluginEntryTest PoseLogicTest ApartLogicTest MooLogicTest SoundLogicTest HordeLogicTest GuardLogicTest SafeguardLogicTest NewsLogicTest WalkerLooksTest LevelingLogicTest QuestVarLogicTest LinkLogicTest SkillLogicTest GrabLogicTest EndLogicTest)
	add_executable(${name} ${name}.cpp ../src/PluginEntry.cpp)
	target_include_directories(${name} PRIVATE ../src)
	target_compile_options(${name} PRIVATE -Wall -Wextra)
	add_test(NAME ${name} COMMAND ${name})
endforeach()
