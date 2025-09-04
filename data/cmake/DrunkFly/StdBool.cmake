
include(CheckIncludeFile)

check_include_file("stdbool.h" HAVE_STDBOOL_H)
if(NOT HAVE_STDBOOL_H)
    include_directories("${CMAKE_CURRENT_LIST_DIR}/stdbool")
endif()
