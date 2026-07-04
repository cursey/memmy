#include "base_core.h"
#include "test_framework.h"

extern TestSuite suite_arena;
extern TestSuite suite_string;
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
extern TestSuite suite_memmy_cli;
extern TestSuite suite_memmy_win32_backend;

int main(int argc, char **argv)
{
    Unused(argc);
    Unused(argv);

    TestSuite suites[] = {
        suite_arena,
        suite_string,
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
        suite_memmy_cli,
        suite_memmy_win32_backend,
    };

    Test_RunAll(suites, ArrayCount(suites));
    return (test__total_fail > 0) ? 1 : 0;
}
