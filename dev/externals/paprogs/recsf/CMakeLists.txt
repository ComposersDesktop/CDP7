if(APPLE)
  set(CMAKE_C_FLAGS "-O2 -Wall -mmacosx-version-min=10.5  -arch i386 -Dunix  -DMAC  -DPA_USE_COREAUDIO -fomit-frame-pointer
  -funroll-loops")
  set(EXTRA_LIBRARIES1 CoreAudio AudioToolbox AudioUnit Carbon m ${EXTRA_LIBARIES})
else()
  if(MINGW)
    set(CMAKE_C_FLAGS "-O3 -DWIN32 -D_WIN32 -DUSE_ASIO -fomit-frame-pointer -funroll-loops")
    set(EXTRA_LIBRARIES1 ${EXTRA_LIBARIES})
  else()
    set(CMAKE_C_FLAGS "-O3 -Wall -Dlinux -Dunix -fomit-frame-pointer -funroll-loops")
    set(EXTRA_LIBRARIES1 asound jack pthread m ${EXTRA_LIBARIES})
  endif()
endif()

link_directories(../lib ../portaudio/lib/.libs)

include_directories(../../../include ../../include ../portaudio/include
                    ../portaudio/src/common ../../)

add_executable(recsf recsf.c)
target_link_libraries(recsf aaio portaudio.a portsf ${EXTRA_LIBRARIES1})

my_install(recsf)
