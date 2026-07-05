file(GLOB_RECURSE memmy_dsl_sources CONFIGURE_DEPENDS src/*.c)

# memmy_dsl parses DSL syntax into ASTs.
add_library(memmy_dsl ${memmy_dsl_sources})
target_include_directories(memmy_dsl PUBLIC include)
target_link_libraries(memmy_dsl PUBLIC memmy)
