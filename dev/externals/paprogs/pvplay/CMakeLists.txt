if(APPLE)
  set(CMAKE_C_FLAGS "-O2 -Wall -mmacosx-version-min=10.5  -arch i386 -Dunix -DMAC -DPA_USE_COREAUDIO -fomit-frame-pointer
  -funroll-loops")
  set(EXTRA_LIBRARIES1 CoreAudio AudioToolbox AudioUnit Carbon ${EXTRA_LIBRARIES})
else()
  if(MINGW)
    set(CMAKE_C_FLAGS "-O3 -DWIN32 -D_WIN32 -DUSE_ASIO -fomit-frame-pointer -funroll-loops")
    set(EXTRA_LIBRARIES1 ${EXTRA_LIBRARIES})
  else()
    set(CMAKE_C_FLAGS "-O3 -Wall -Dlinux -Dunix -fomit-frame-pointer -funroll-loops")
    set(EXTRA_LIBRARIES1 asound jack pthread ${EXTRA_LIBRARIES})
  endif()
endif()

link_directories(../lib ../portaudio/lib/.libs)

include_directories(../../../include ../include ../portaudio/include
                    ../portaudio/src/common)

add_executable(pvplay pvplay.cpp pvoc2.cpp fmhfuncs.c pvthreads.cpp pvfileio.c mxfft.c)
target_link_libraries(pvplay aaio portaudio.a sfsys ${EXTRA_LIBRARIES1})

my_install(pvplay)
