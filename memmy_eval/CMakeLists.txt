file(GLOB_RECURSE memmy_eval_sources CONFIGURE_DEPENDS src/*.c)

add_library(memmy_eval ${memmy_eval_sources})
target_include_directories(memmy_eval PUBLIC include)
target_link_libraries(memmy_eval PUBLIC memmy_ast memmy)
