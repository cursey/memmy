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
extern TestSuite suite_memmy;

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
        suite_memmy,
    };

    Test_RunAll(suites, ArrayCount(suites));
    return (test__total_fail > 0) ? 1 : 0;
}
