if(APPLE)
  set(CMAKE_C_FLAGS "-O2 -Wall -mmacosx-version-min=10.5  -arch i386 -Dunix -fomit-frame-pointer -funroll-loops")
  set(EXTRA_LIBRARIES1 jack asound portsf pthread ${EXTRA_LIBRARIES})
else()
  if(MINGW)
    set(CMAKE_C_FLAGS "-O3 -DWIN32 -D_WIN32 -fomit-frame-pointer  -funroll-loops")
    set(EXTRA_LIBRARIES1 winmm dsound winspool ${EXTRA_LIBRARIES})
  else()
    set(CMAKE_C_FLAGS "-O3 -Wall -Dlinux -Dunix -fomit-frame-pointer -funroll-loops")
    set(EXTRA_LIBRARIES1 jack asound portsf pthread ${EXTRA_LIBRARIES})
  endif()
endif()

link_directories(../lib ../portaudio/lib/.libs)

include_directories(../../../include ../include ../portaudio/include
                    ../portaudio/src/common )

add_executable(listaudevs devs.c)
target_link_libraries(listaudevs portaudio.a ${EXTRA_LIBRARIES1})

my_install(listaudevs)
