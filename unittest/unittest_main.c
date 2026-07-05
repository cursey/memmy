#include "base_core.h"
#include "test_framework.h"

#include <string.h>

extern TestSuite suite_arena;
extern TestSuite suite_string;
extern TestSuite suite_memory;
extern TestSuite suite_math;
extern TestSuite suite_string_split;
extern TestSuite suite_bitset;
extern TestSuite suite_sort;
extern TestSuite suite_list;
extern TestSuite suite_hash;
extern TestSuite suite_avl_u64;
extern TestSuite suite_avl_str8;
extern TestSuite suite_hashmap;
extern TestSuite suite_regex;
extern TestSuite suite_fs;
extern TestSuite suite_process;
extern TestSuite suite_checked;
extern TestSuite suite_memmy_header;
extern TestSuite suite_memmy_status;
extern TestSuite suite_memmy_value;
extern TestSuite suite_memmy_range;
extern TestSuite suite_memmy_scan;
extern TestSuite suite_memmy_context;
extern TestSuite suite_memmy_process;
extern TestSuite suite_memmy_dsl_target;
extern TestSuite suite_memmy_dsl_const;
extern TestSuite suite_memmy_dsl_address;
extern TestSuite suite_memmy_dsl_memory;
extern TestSuite suite_memmy_dsl_range;
extern TestSuite suite_memmy_dsl_statement;
extern TestSuite suite_memmy_exec_address;
extern TestSuite suite_memmy_exec_peek_poke;
extern TestSuite suite_memmy_exec_range;
extern TestSuite suite_memmy_exec_statement;
extern TestSuite suite_memmy_exec_pattern_scan;
extern TestSuite suite_memmy_exec_value_scan;
extern TestSuite suite_memmy_cli;
extern TestSuite suite_memmy_cli_dsl;
extern TestSuite suite_memmy_cli_repl;
extern TestSuite suite_memmy_default_backend;

int main(int argc, char **argv)
{
    TestSuite suites[] = {
        suite_arena,
        suite_string,
        suite_memory,
        suite_math,
        suite_string_split,
        suite_bitset,
        suite_sort,
        suite_list,
        suite_hash,
        suite_avl_u64,
        suite_avl_str8,
        suite_hashmap,
        suite_regex,
        suite_fs,
        suite_process,
        suite_checked,
        suite_memmy_header,
        suite_memmy_status,
        suite_memmy_value,
        suite_memmy_range,
        suite_memmy_scan,
        suite_memmy_context,
        suite_memmy_process,
        suite_memmy_dsl_target,
        suite_memmy_dsl_const,
        suite_memmy_dsl_address,
        suite_memmy_dsl_memory,
        suite_memmy_dsl_range,
        suite_memmy_dsl_statement,
        suite_memmy_exec_address,
        suite_memmy_exec_peek_poke,
        suite_memmy_exec_range,
        suite_memmy_exec_statement,
        suite_memmy_exec_pattern_scan,
        suite_memmy_exec_value_scan,
        suite_memmy_cli,
        suite_memmy_cli_dsl,
        suite_memmy_cli_repl,
        suite_memmy_default_backend,
    };

    if (argc == 2 && strcmp(argv[1], "--list-cases") == 0)
    {
        Test_ListAll(suites, ArrayCount(suites));
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "--case") == 0)
    {
        return Test_RunCase(suites, ArrayCount(suites), argv[2]) ? 0 : 1;
    }

    Test_RunAll(suites, ArrayCount(suites));
    return (test__total_fail > 0) ? 1 : 0;
}
