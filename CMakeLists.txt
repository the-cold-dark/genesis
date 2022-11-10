cmake_minimum_required(VERSION 3.16)
project(GENESIS)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/include)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/modules)
INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR})

SET(VERSION_MAJOR 1)
SET(VERSION_MINOR 2)
SET(VERSION_PATCH 0)
SET(VERSION_RELEASE "DEV")

SET(RESTRICTIVE_FILES ON CACHE BOOL "File operations may be restricted.")
SET(CACHE_WIDTH 61 CACHE STRING "Width to use for the object cache. This should be a prime number. Default is 61.")
SET(CACHE_DEPTH 10 CACHE STRING "Depth to use for the object cache. Default is 10.")
SET(USE_CLEANER_THREAD OFF CACHE BOOL "EXPERIMENTAL: Use a thread for cleaning the cache.")
SET(DEBUG_DB_LOCK OFF CACHE BOOL "Debug option for USE_CLEANER_THREAD")
SET(DEBUG_LOOKUP_LOCK OFF CACHE BOOL "Debug option for USE_CLEANER_THREAD")
SET(DEBUG_BUCKET_LOCK OFF CACHE BOOL "Debug option for USE_CLEANER_THREAD")
SET(DEBUG_CLEANER_LOCK OFF CACHE BOOL "Debug option for USE_CLEANER_THREAD")
SET(USE_PARENT_OBJS OFF CACHE BOOL "EXPERIMENTAL: still in development.")
SET(COLD_LOOKUP_BACKEND "ndbm" CACHE STRING "Backend to use for lookup: ndbm, bdb.")

INCLUDE(${CMAKE_SOURCE_DIR}/Modules/GetTriple.cmake)
GET_TARGET_TRIPLE(SYSTEM_TYPE TARGET_ARCH TARGET_VENDOR TARGET_OS)

INCLUDE(${CMAKE_ROOT}/Modules/CheckFunctionExists.cmake)
INCLUDE(${CMAKE_ROOT}/Modules/CheckIncludeFile.cmake)
INCLUDE(${CMAKE_ROOT}/Modules/CheckLibraryExists.cmake)
INCLUDE(${CMAKE_ROOT}/Modules/CheckTypeSize.cmake)

INCLUDE(${CMAKE_SOURCE_DIR}/Modules/AddCCompilerFlag.cmake)
ADD_C_COMPILER_FLAG(-Wall)

# clang doesn't print colored diagnostics when invoked from Ninja
IF (UNIX AND
    CMAKE_C_COMPILER_ID STREQUAL "Clang" AND
    CMAKE_GENERATOR STREQUAL "Ninja")
  SET(CMAKE_C_FLAGS "-fcolor-diagnostics ${CMAKE_C_FLAGS}")
ENDIF()

CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)

SET(COLD_LIBRARIES)

# Do we need libm?
CHECK_LIBRARY_EXISTS(m sin "" LINK_LIBM)
IF(LINK_LIBM)
  SET(COLD_LIBRARIES
      ${COLD_LIBRARIES}
      -lm)
ENDIF()

IF(COLD_LOOKUP_BACKEND STREQUAL "ndbm")
  # Look for ndbm.h. This will either come from ndbm or gdbm-compat.
  # Then, figure out which library, if any, needs to be linked.
  CHECK_INCLUDE_FILE(ndbm.h HAVE_NDBM_H)
  IF(HAVE_NDBM_H)
    # On macOS, we don't have to link against anything else.
    CHECK_FUNCTION_EXISTS(dbm_open HAVE_DBM_OPEN)
    IF(NOT HAVE_DBM_OPEN)
      CHECK_LIBRARY_EXISTS(ndbm dbm_open "" LINK_NDBM)
      IF(LINK_NDBM)
        SET(COLD_LIBRARIES ${COLD_LIBRARIES} -lndbm)
      ELSE()
        CHECK_LIBRARY_EXISTS(gdbm_compat dbm_open "" LINK_GDBM_COMPAT)
        IF(LINK_GDBM_COMPAT)
          SET(COLD_LIBRARIES ${COLD_LIBRARIES} -lgdbm_compat)
        ENDIF()
      ENDIF()
    ENDIF()
    SET(src_LOOKUP src/lookup_ndbm.c)
  ELSE()
    MESSAGE(FATAL_ERROR "You must have ndbm or gdbm's ndbm emulation (gdbm-compat) installed.")
  ENDIF()
ENDIF()

IF(COLD_LOOKUP_BACKEND STREQUAL "bdb")
  CHECK_INCLUDE_FILE(db.h HAVE_DB_H)
  IF(HAVE_DB_H)
    CHECK_LIBRARY_EXISTS(db db_create "" LINK_DB)
    IF(LINK_DB)
      SET(COLD_LIBRARIES ${COLD_LIBRARIES} -ldb)
      SET(src_LOOKUP src/lookup_bdb.c)
    ENDIF()
  ENDIF()
  IF(NOT HAVE_DB_H OR NOT LINK_DB)
    MESSAGE(FATAL_ERROR "You must have berkeley db (libdb and libdb-dev) installed.")
  ENDIF()
ENDIF()

IF(UNIX)
  SET(__UNIX__ 1)
ENDIF()

IF(WINDOWS)
  SET(__Win32__ 1)
ENDIF()

CHECK_TYPE_SIZE("long" SIZEOF_LONG)
CHECK_TYPE_SIZE("float" SIZEOF_FLOAT)

CHECK_FUNCTION_EXISTS(getrusage HAVE_GETRUSAGE)
CHECK_FUNCTION_EXISTS(gettimeofday HAVE_GETTIMEOFDAY)

CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/src/include/config.h.cmake
               ${CMAKE_BINARY_DIR}/config.h)


FIND_PACKAGE(BISON)
BISON_TARGET(ColdParser src/grammar.y ${CMAKE_BINARY_DIR}/parse.c)

SET(MODULE_GENERATED_HEADER ${CMAKE_SOURCE_DIR}/src/modules/moddef.h)
SET(MODULE_CONFIGURATION
    cdc
    ext_math
    web)
SET(MODULE_DEPENDENCIES)
FOREACH(MOD IN LISTS MODULE_CONFIGURATION)
    LIST(APPEND MODULE_DEPENDENCIES "${CMAKE_SOURCE_DIR}/src/modules/${MOD}.mod")
ENDFOREACH()
ADD_CUSTOM_COMMAND(
    OUTPUT ${MODULE_GENERATED_HEADER}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src/modules
    COMMAND ./modbuild ${MODULE_CONFIGURATION}
    DEPENDS ${CMAKE_SOURCE_DIR}/src/modules/modbuild
            ${MODULE_DEPENDENCIES}
)

SET(src_CRYPT
    src/crypt.c
    src/shs.c)
SET(src_DATA
    src/data/data.c
    src/data/buffer.c
    src/data/dict.c
    src/data/ident.c
    src/data/list.c
    src/data/object.c
    src/data/string.c
    src/data/string_tab.c
    src/data/handled_frob.c
    src/data/quickhash.c)
SET(src_DB
    src/cache.c
    src/binarydb.c
    src/dbpack.c
    src/decode.c)
SET(src_GRAMMAR
    ${BISON_ColdParser_OUTPUTS})
SET(src_IO
    src/file.c
    src/io.c
    src/log.c
    src/net.c)
SET(src_MISC
    src/defs.c
    src/dns.c
    src/memory.c
    src/regexp.c
    src/sig.c
    src/strutil.c
    src/util.c)
SET(src_OPS
    src/ops/buffer.c
    src/ops/data.c
    src/ops/dict.c
    src/ops/error.c
    src/ops/file.c
    src/ops/list.c
    src/ops/math.c
    src/ops/misc.c
    src/ops/network.c
    src/ops/object.c
    src/ops/operators.c
    src/ops/string.c
    src/ops/sys.c
    src/ops/task.c)
SET(src_PCODE
    src/codegen.c
    src/execute.c
    src/native.c
    src/opcodes.c
    src/token.c)
SET(src_MOD
    ${MODULE_GENERATED_HEADER}
    src/modules/cdc.c
    src/modules/cdc_buffer.c
    src/modules/cdc_dict.c
    src/modules/cdc_list.c
    src/modules/cdc_misc.c
    src/modules/cdc_string.c
    src/modules/cdc_integer.c
    src/modules/ext_math.c
    src/modules/web.c)

SET(src_COMMON
    ${src_CRYPT}
    ${src_DATA}
    ${src_DB}
    ${src_LOOKUP}
    ${src_GRAMMAR}
    ${src_IO}
    ${src_MISC}
    ${src_MOD}
    ${src_OPS}
    ${src_PCODE})

SET(src_DRIVER
    src/genesis.c
    ${src_COMMON})

SET(src_COMPILER
    src/coldcc.c
    src/textdb.c
    ${src_COMMON})

ADD_EXECUTABLE(genesis ${src_DRIVER})
ADD_EXECUTABLE(coldcc ${src_COMPILER})
TARGET_COMPILE_DEFINITIONS(coldcc PRIVATE BUILDING_COLDCC)
TARGET_LINK_LIBRARIES(genesis ${COLD_LIBRARIES})
TARGET_LINK_LIBRARIES(coldcc ${COLD_LIBRARIES})

INCLUDE(CTest)
ADD_TEST(
    NAME legacy
    COMMAND ./runtest
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test/legacy
)
ADD_TEST(
    NAME dicts
    COMMAND ./runtest cdc/dicts.cdc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
)
ADD_TEST(
    NAME lists
    COMMAND ./runtest cdc/lists.cdc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
)
ADD_TEST(
    NAME math
    COMMAND ./runtest cdc/math.cdc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
)
ADD_TEST(
    NAME strings
    COMMAND ./runtest cdc/strings.cdc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
)
ADD_TEST(
    NAME symbols
    COMMAND ./runtest cdc/symbols.cdc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
)
ADD_TEST(
    NAME system
    COMMAND ./runtest cdc/system.cdc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
)
