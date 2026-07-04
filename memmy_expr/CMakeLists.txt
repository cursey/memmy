file(GLOB_RECURSE memmy_expr_sources CONFIGURE_DEPENDS src/*.c)

add_library(memmy_expr ${memmy_expr_sources})
target_include_directories(memmy_expr PUBLIC include)
target_link_libraries(memmy_expr PUBLIC memmy)
