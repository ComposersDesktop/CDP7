if(APPLE)
  set(CMAKE_C_FLAGS "-O2 -Wall -mmacosx-version-min=10.8 -Dunix -fomit-frame-pointer -funroll-loops -stdlib=libc++")
  set(CMAKE_CXX_FLAGS "-O2 -Wall -mmacosx-version-min=10.8 -Dunix -fomit-frame-pointer -funroll-loops -std=c++11 -stdlib=libc++")
else()
  if(MINGW)
    set(CMAKE_C_FLAGS "-O3 -DWIN32 -D_WIN32 -fomit-frame-pointer  -funroll-loops")
    set(CMAKE_CXX_FLAGS "-O3 -DWIN32 -D_WIN32 -fomit-frame-pointer  -funroll-loops -static-libgcc -static-libstdc++")

  else()
    set(CMAKE_C_FLAGS "-O3 -Wall -Dlinux -Dunix -fomit-frame-pointer -funroll-loops")
  endif()
endif ()

link_directories(../lib)

include_directories(../../../include ../include)
  
add_executable(rmverb rmverb.cpp reverberator.cpp wavetable.cpp)
target_link_libraries(rmverb portsf sfsys ${EXTRA_LIBRARIES})
my_install(rmverb)

add_executable(reverb reverb.cpp reverberator.cpp wavetable.cpp)
target_link_libraries(reverb portsf sfsys ${EXTRA_LIBRARIES})
my_install(reverb)

add_executable(rmresp roomresp.cpp)
target_link_libraries(rmresp portsf sfsys ${EXTRA_LIBRARIES})
my_install(rmresp)

add_executable(tapdelay tdelaymain.cpp reverberator.cpp wavetable.cpp)
target_link_libraries(tapdelay portsf sfsys ${EXTRA_LIBRARIES})
my_install(tapdelay)
