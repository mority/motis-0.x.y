set(CMAKE_C_COMPILER /usr/bin/clang-14)
set(CMAKE_CXX_COMPILER /usr/bin/clang++-14)

set(CMAKE_C_FLAGS "-fcolor-diagnostics" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "-fcolor-diagnostics -stdlib=libc++" CACHE STRING "" FORCE)