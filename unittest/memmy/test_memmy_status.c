#include "test_memmy_common.h"

Test(Test_MemmyStatusAndErrorHelpers)
{
    AssertStrEq(Memmy_Status_String(Memmy_Status_ParseError), String8_Lit("parse_error"));
    AssertStrEq(Memmy_Status_String((Memmy_Status)9999), String8_Lit("unknown"));

    Memmy_Error error = {0};
    Memmy_Error_Set(&error, Memmy_Status_Unsupported, String8_Lit("backend"), String8_Lit("no callback"));
    AssertEq(error.status, Memmy_Status_Unsupported);
    AssertStrEq(error.context, String8_Lit("backend"));
    AssertStrEq(error.message, String8_Lit("no callback"));
}

TestSuite suite_memmy_status = TestSuite_Make("Memmy Status", TestCase_Make(Test_MemmyStatusAndErrorHelpers), );
