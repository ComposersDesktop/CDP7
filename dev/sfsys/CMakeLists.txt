if(APPLE)
  set(CMAKE_C_FLAGS "-O2 -mmacosx-version-min=10.5 -arch i386 -arch x86_64 -DCDP97 -Dunix -D__MAC__  -DFILE_AMB_SUPPORT")
else()
  if(MINGW)
    set(CMAKE_C_FLAGS "-O2 -DCDP97 -DCDP99 -DFILE64_WIN -D_X86_ -DWIN32 -D_WIN32 -D__GNUWIN32__ -DFILE_AMB_SUPPORT")
  else()
    set(CMAKE_C_FLAGS "-O2 -DCDP97 -Dunix -Dlinux -D_X86_  -DFILE_AMB_SUPPORT")
  endif()
endif()

include_directories(../../include)

set(SFSYS_SRCS sfsys.c snd.c sfdir.c osbind.c props.c)

add_library(sfsys ${SFSYS_SRCS})
