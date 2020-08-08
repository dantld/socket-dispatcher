
project(server)

set(CXXFLAGS -ggdb)

add_executable(mylsnr mylsnr.cpp utils.cpp)

add_executable(mydisp mydisp.cpp utils.cpp)
