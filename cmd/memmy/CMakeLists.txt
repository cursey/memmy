add_executable(cmd_memmy memmy_main.c memmy_cli.c)
target_link_libraries(cmd_memmy PRIVATE memmy)
set_target_properties(cmd_memmy PROPERTIES OUTPUT_NAME memmy)
