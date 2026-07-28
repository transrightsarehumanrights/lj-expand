cmake_minimum_required(VERSION 3.20)
project(lje C)

# CMake code was highly adapted from the original msvcbuild.bat file.

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "LJE is x64/GC64 only; configure a 64-bit toolchain.")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(LJE_BUILD_LUAJIT   "Build the standalone luajit CLI" ON) # TODO: Probably get rid of this? Useful for AI agents to test it though.
option(LJE_BUILD_LAUNCHER "Build lje-launcher" ON)

set(LJ_SRC  ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(LJ_DASM ${CMAKE_CURRENT_SOURCE_DIR}/dynasm)
set(LJ_GEN  ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${LJ_GEN})

if(NOT WIN32)
  enable_language(ASM)
endif()

# This is because anyone who builds LJE without the new CMake commit will have these and it'll
# mess up the new CMake builds.
foreach(stale lj_bcdef.h lj_ffdef.h lj_libdef.h lj_recdef.h lj_folddef.h host/buildvm_arch.h)
  if(EXISTS ${LJ_SRC}/${stale})
    message(WARNING "Stale generated header src/${stale} will shadow the CMake build. Delete it.")
  endif()
endforeach()

set(LJ_DEFINES LUAJIT_DISABLE_VMEVENT LUAJIT_DISABLE_FFI LUAJIT_ENABLE_GC64) # GMod flags

if(MSVC)
  list(APPEND LJ_DEFINES _CRT_SECURE_NO_DEPRECATE)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
  add_compile_options(/W3)
else()
  add_compile_options(-Wall -fomit-frame-pointer)
endif()

set(LJ_DASM_FLAGS -D JIT -D P64)
if(WIN32)
  list(APPEND LJ_DASM_FLAGS -D WIN)
endif()

# Below are all host tooling, nothing to do with the library itself.
add_executable(minilua ${LJ_SRC}/host/minilua.c)
target_compile_definitions(minilua PRIVATE ${LJ_DEFINES})
if(UNIX)
  target_link_libraries(minilua PRIVATE m)
endif()

add_custom_command(
  OUTPUT ${LJ_GEN}/buildvm_arch.h
  COMMAND $<TARGET_FILE:minilua> ${LJ_DASM}/dynasm.lua -LN ${LJ_DASM_FLAGS}
          -o ${LJ_GEN}/buildvm_arch.h vm_x64.dasc
  DEPENDS minilua ${LJ_SRC}/vm_x64.dasc
  WORKING_DIRECTORY ${LJ_SRC}
  COMMENT "DynASM vm_x64.dasc -> buildvm_arch.h")

add_executable(buildvm
  ${LJ_SRC}/host/buildvm.c
  ${LJ_SRC}/host/buildvm_asm.c
  ${LJ_SRC}/host/buildvm_fold.c
  ${LJ_SRC}/host/buildvm_lib.c
  ${LJ_SRC}/host/buildvm_peobj.c
  ${LJ_GEN}/buildvm_arch.h)
target_include_directories(buildvm PRIVATE ${LJ_SRC} ${LJ_DASM} ${LJ_GEN})
target_compile_definitions(buildvm PRIVATE ${LJ_DEFINES})

add_executable(lua2c ${LJ_SRC}/lua2c.c)
target_compile_definitions(lua2c PRIVATE ${LJ_DEFINES})

# LuaJIT compiles its VM manually since it's hand-written assembly, so we link into it from CMake
# as a generated external object.
if(WIN32)
  set(LJ_VM_BOUT ${LJ_GEN}/lj_vm.obj)
  set(LJ_VM_MODE peobj)
else()
  set(LJ_VM_BOUT ${LJ_GEN}/lj_vm.S)
  set(LJ_VM_MODE elfasm)
endif()

add_custom_command(
  OUTPUT ${LJ_VM_BOUT}
  COMMAND $<TARGET_FILE:buildvm> -m ${LJ_VM_MODE} -o ${LJ_VM_BOUT}
  DEPENDS buildvm
  WORKING_DIRECTORY ${LJ_SRC}
  COMMENT "buildvm -m ${LJ_VM_MODE}")

if(WIN32)
  set_source_files_properties(${LJ_VM_BOUT} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
else()
  set_source_files_properties(${LJ_VM_BOUT} PROPERTIES LANGUAGE ASM GENERATED TRUE)
endif()

set(LJ_ALL_LIB lib_base.c lib_math.c lib_bit.c lib_string.c lib_table.c
               lib_io.c lib_os.c lib_package.c lib_debug.c lib_jit.c lib_ffi.c)
set(LJ_ALL_LIB_ABS)
foreach(f ${LJ_ALL_LIB})
  list(APPEND LJ_ALL_LIB_ABS ${LJ_SRC}/${f})
endforeach()

set(LJ_GEN_HDRS)
foreach(def bcdef ffdef libdef recdef)
  add_custom_command(
    OUTPUT ${LJ_GEN}/lj_${def}.h
    COMMAND $<TARGET_FILE:buildvm> -m ${def} -o ${LJ_GEN}/lj_${def}.h ${LJ_ALL_LIB}
    DEPENDS buildvm ${LJ_ALL_LIB_ABS}
    WORKING_DIRECTORY ${LJ_SRC}
    COMMENT "buildvm -m ${def}")
  list(APPEND LJ_GEN_HDRS ${LJ_GEN}/lj_${def}.h)
endforeach()

add_custom_command(
  OUTPUT ${LJ_GEN}/lj_folddef.h
  COMMAND $<TARGET_FILE:buildvm> -m folddef -o ${LJ_GEN}/lj_folddef.h lj_opt_fold.c
  DEPENDS buildvm ${LJ_SRC}/lj_opt_fold.c
  WORKING_DIRECTORY ${LJ_SRC}
  COMMENT "buildvm -m folddef")
list(APPEND LJ_GEN_HDRS ${LJ_GEN}/lj_folddef.h)

# vmdef.lua is runtime data loaded from jit/
add_custom_command(
  OUTPUT ${LJ_SRC}/jit/vmdef.lua
  COMMAND $<TARGET_FILE:buildvm> -m vmdef -o ${LJ_SRC}/jit/vmdef.lua ${LJ_ALL_LIB}
  DEPENDS buildvm ${LJ_ALL_LIB_ABS}
  WORKING_DIRECTORY ${LJ_SRC}
  COMMENT "buildvm -m vmdef")
add_custom_target(lje-vmdef DEPENDS ${LJ_SRC}/jit/vmdef.lua)

# All embedded Lua is lua2c'd then copied into the generated/ directory.
foreach(script lje_secure_preinit lje_helpers)
  add_custom_command(
    OUTPUT ${LJ_SRC}/generated/${script}.h
    COMMAND $<TARGET_FILE:lua2c> lua/${script}.lua generated/${script}.h ${script}
    DEPENDS lua2c ${LJ_SRC}/lua/${script}.lua
    WORKING_DIRECTORY ${LJ_SRC}
    COMMENT "lua2c ${script}.lua")
  list(APPEND LJ_GEN_HDRS ${LJ_SRC}/generated/${script}.h)
endforeach()

# LJE itself! Note all vendor libraries must be compiled into the shared library, since we can't ship them as separate DLLs.
file(GLOB LJ_CORE_SRC CONFIGURE_DEPENDS ${LJ_SRC}/lj_*.c ${LJ_SRC}/lib_*.c)

# The platform backends are mutually exclusive. They also self-guard on
# LJE_PLATFORM_WINDOWS, so this is belt-and-braces, but it keeps the wrong one
# out of the compile database and off the IDE's radar.
if(WIN32)
  list(REMOVE_ITEM LJ_CORE_SRC ${LJ_SRC}/lj_expand_platform_posix.c)
else()
  list(REMOVE_ITEM LJ_CORE_SRC ${LJ_SRC}/lj_expand_platform_windows.c)
endif()

# MinHook is no longer built: lje_detour_hook does suspend/resume in portable C.
# The vendored tree stays in git as a reference if trampolines ever come back.
set(LJ_VENDOR_SRC ${LJ_SRC}/libs/tomlc17/src/tomlc17.c)

add_library(lje SHARED ${LJ_CORE_SRC} ${LJ_VENDOR_SRC} ${LJ_VM_BOUT} ${LJ_GEN_HDRS})
add_dependencies(lje lje-vmdef)

if(WIN32)
  set_target_properties(lje PROPERTIES OUTPUT_NAME lje-w64)
else()
  set_target_properties(lje PROPERTIES OUTPUT_NAME lje)
endif()
set_target_properties(lje PROPERTIES PREFIX "" POSITION_INDEPENDENT_CODE ON) # Necessary for Linux

target_include_directories(lje PUBLIC ${LJ_SRC} PRIVATE
  ${LJ_GEN}
  ${LJ_SRC}/libs/tomlc17/src
  ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)

target_compile_definitions(lje PRIVATE ${LJ_DEFINES})

if(WIN32)
  target_compile_definitions(lje PRIVATE LUA_BUILD_AS_DLL)
  target_compile_options(lje PRIVATE "/D_CRT_STDIO_INLINE=__declspec(dllexport)__inline") # Very important for MSVC.
  target_link_libraries(lje PRIVATE
    kernel32 user32 crypt32 advapi32 dbghelp shell32)
else()
  target_link_libraries(lje PRIVATE dl m pthread)
endif()

# NOTE: This is not used at all in production, nor even in debugging, but it is incredibly useful
# for AI agents to quickly validate any LJE changes they make. It does not run anything, but it initializes LJE
# within a separate process and then exits, which is enough to validate that the build is working.
if(LJE_BUILD_LUAJIT)
  add_executable(luajit ${LJ_SRC}/luajit.c)
  target_link_libraries(luajit PRIVATE lje)
  target_compile_definitions(luajit PRIVATE ${LJ_DEFINES})
  if(NOT WIN32)
    target_link_options(luajit PRIVATE -Wl,-E)  # C modules resolve lua_* from the exe
  endif()
endif()

if(LJE_BUILD_LAUNCHER AND WIN32)
  file(GLOB LJEL_SRC CONFIGURE_DEPENDS ${LJ_SRC}/launcher/*.c)
  add_executable(lje-launcher ${LJEL_SRC})
  target_include_directories(lje-launcher PRIVATE ${LJ_SRC}/launcher)
  target_compile_definitions(lje-launcher PRIVATE ${LJ_DEFINES})
  target_link_libraries(lje-launcher PRIVATE lje user32 kernel32)
endif()
