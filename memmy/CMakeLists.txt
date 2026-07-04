file(GLOB_RECURSE memmy_sources CONFIGURE_DEPENDS src/*.c)

add_library(memmy ${memmy_sources})
target_include_directories(memmy PUBLIC include)
target_link_libraries(memmy PUBLIC base)
