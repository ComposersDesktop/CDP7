if(APPLE)
  set(CMAKE_C_FLAGS "-O2 -Wall -mmacosx-version-min=10.5  -arch i386 -Dunix -DIS_CDPARSE -D__MAC__")
else()
  if(MINGW)
    set(CMAKE_C_FLAGS "-O2 -Wall -DWIN32 -D_WIN32 -DIS_CDPARSE")
  else()
    set(CMAKE_C_FLAGS "-O2 -Wall -Dlinux -Dunix -DIS_CDPARSE")
  endif()
endif()

link_directories(../cdp2k ../sfsys)

include_directories(../../include)

add_executable(cdparse cdparse.c)

target_link_libraries(cdparse cdp2k sfsys ${EXTRA_LIBRARIES})

my_install(cdparse)
