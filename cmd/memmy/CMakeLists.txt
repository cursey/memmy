add_executable(cmd_memmy
    memmy_main.c
    memmy_cli.c
    memmy_cli_procs.c
    memmy_cli_peek.c
    memmy_cli_poke.c
    memmy_cli_scan.c
    memmy_cli_mods.c
    memmy_cli_regions.c
)
target_link_libraries(cmd_memmy PRIVATE memmy)
set_target_properties(cmd_memmy PROPERTIES OUTPUT_NAME memmy)
