file(GLOB_RECURSE memmy_exec_sources CONFIGURE_DEPENDS src/*.c)

add_library(memmy_exec ${memmy_exec_sources})
target_include_directories(memmy_exec PUBLIC include)
# memmy_exec executes parsed DSL ASTs and emits structured results/events.
target_link_libraries(memmy_exec PUBLIC memmy_dsl memmy)
