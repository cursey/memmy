add_library(vendor_zydis OBJECT zydis/Zydis.c)
target_include_directories(vendor_zydis PUBLIC zydis)
target_compile_definitions(vendor_zydis PUBLIC ZYDIS_STATIC_BUILD)
