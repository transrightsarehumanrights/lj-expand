cmake_minimum_required(VERSION 3.15)
project(lj-expand C)

file(GLOB_RECURSE SOURCES "src/*.c" "src/*.h")
add_executable(luajit_dummy ${SOURCES})
add_compile_options(luajit_dummy PRIVATE -DLUAJIT_DISABLE_VMEVENT -DLUAJIT_ENABLE_GC64 -DLUAJIT_DISABLE_FFI)
set_target_properties(luajit_dummy PROPERTIES EXCLUDE_FROM_ALL TRUE)