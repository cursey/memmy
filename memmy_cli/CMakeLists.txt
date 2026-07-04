file(GLOB_RECURSE memmy_cli_sources CONFIGURE_DEPENDS src/*.c)

add_library(memmy_cli ${memmy_cli_sources})
target_include_directories(memmy_cli PUBLIC include)
target_include_directories(memmy_cli PRIVATE src)
target_link_libraries(memmy_cli PUBLIC memmy)
