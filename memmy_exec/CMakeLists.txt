file(GLOB_RECURSE memmy_exec_sources CONFIGURE_DEPENDS src/*.c)

add_library(memmy_exec ${memmy_exec_sources})
target_include_directories(memmy_exec PUBLIC include)
target_link_libraries(memmy_exec PUBLIC memmy_expr memmy)
