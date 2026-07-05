file(GLOB_RECURSE memmy_ast_sources CONFIGURE_DEPENDS src/*.c)

add_library(memmy_ast ${memmy_ast_sources})
target_include_directories(memmy_ast PUBLIC include)
target_link_libraries(memmy_ast PUBLIC base)
