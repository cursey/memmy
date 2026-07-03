add_library(memmy INTERFACE)
target_include_directories(memmy INTERFACE include)
target_link_libraries(memmy INTERFACE base)
