file(GLOB_RECURSE MEMMY_SOURCES CONFIGURE_DEPENDS src/*.c)

add_library(memmy ${MEMMY_SOURCES})
target_include_directories(memmy PUBLIC include)
target_link_libraries(memmy PUBLIC base)
