file(GLOB_RECURSE base_sources CONFIGURE_DEPENDS src/*.c)

add_library(base STATIC ${base_sources})
target_include_directories(base PUBLIC include)
target_link_libraries(base PUBLIC vendor_zydis)
