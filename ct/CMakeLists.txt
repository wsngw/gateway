cmake_minimum_required(VERSION 2.8)
project(gw)

#set(CMAKE_BUILD_TYPE Debug)
if(CMAKE_BUILD_TYPE MATCHES Debug)
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGW_DEBUG")
endif(CMAKE_BUILD_TYPE MATCHES Debug)

SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_HOME_DIRECTORY}/bin/${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME})
SET(LIBRARY_OUTPUT_PATH ${CMAKE_HOME_DIRECTORY}/bin/${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME})

if(CMAKE_SYSTEM_NAME MATCHES Linux)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES arm)
    include_directories(BEFORE ${CMAKE_HOME_DIRECTORY}/include/platform/arm-linux-gcc)
  endif(CMAKE_SYSTEM_PROCESSOR MATCHES arm)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES i386)
    include_directories(BEFORE ${CMAKE_HOME_DIRECTORY}/include/platform/i386-linux-gcc)
  endif(CMAKE_SYSTEM_PROCESSOR MATCHES i386)
endif(CMAKE_SYSTEM_NAME MATCHES Linux)

include_directories(BEFORE ${CMAKE_HOME_DIRECTORY}/include)

IF(CMAKE_COMPILER_IS_GNUCC)
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fmessage-length=0")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

#build ligwsngw.so
aux_source_directory(src/common LIBWSNGW_COMMON_SRC)
aux_source_directory(src/common/dotconf LIBWSNGW_COMMON_DOTCONF_SRC)
aux_source_directory(src/common/httpd LIBWSNGW_COMMON_HTTPD_SRC)
aux_source_directory(src/common/json LIBWSNGW_COMMON_JSON_SRC)
aux_source_directory(src/common/rpc LIBWSNGW_COMMON_RPC_SRC)
aux_source_directory(src/common/libevent LIBWSNGW_COMMON_LIBEVENT_SRC)
if(CMAKE_SYSTEM_NAME MATCHES Linux)
  set(LIBWSNGW_PLAT_SRC src/platform/linux/gwdlib.c src/platform/linux/gwmutex.c src/platform/linux/gwsem.c src/platform/linux/gwthread.c)
endif(CMAKE_SYSTEM_NAME MATCHES Linux)
add_library(wsngw SHARED ${LIBWSNGW_COMMON_SRC} ${LIBWSNGW_COMMON_DOTCONF_SRC} ${LIBWSNGW_COMMON_HTTPD_SRC} ${LIBWSNGW_COMMON_JSON_SRC} ${LIBWSNGW_COMMON_RPC_SRC} ${LIBWSNGW_COMMON_LIBEVENT_SRC} ${LIBWSNGW_PLAT_SRC} )
target_link_libraries(wsngw dl pthread m rt)

#build gw
add_executable(gw src/platform/linux/gw.c)
target_link_libraries(gw wsngw)

#build modules
#add_subdirectory(modules)
