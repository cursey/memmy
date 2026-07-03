file(GLOB_RECURSE BASE_SOURCES CONFIGURE_DEPENDS src/*.c)

add_library(base OBJECT ${BASE_SOURCES})
target_include_directories(base PUBLIC include)
target_link_libraries(base PUBLIC vendor_zydis)
