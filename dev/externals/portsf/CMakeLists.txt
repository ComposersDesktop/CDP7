if(APPLE)
  set(CMAKE_C_FLAGS "-O2 -Wall -mmacosx-version-min=10.8 -Dunix")
else()
  if(MINGW)
    set(CMAKE_C_FLAGS "-O2")
  else()
    set(CMAKE_C_FLAGS "-O2 -Dunix -Dlinux")
  endif()
endif()

include_directories(../include)
set(PORTSF_SRCS portsf.c ieee80.c)

add_library(portsf ${PORTSF_SRCS})
