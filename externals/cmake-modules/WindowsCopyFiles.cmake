# Copyright 2016 Citra Emulator Project
# Licensed under GPLv2 or any later version
# Refer to the license.txt file included.

# This file provides the function windows_copy_files.
# This is only valid on Windows.

# Include guard
if(__windows_copy_files)
	return()
endif()
set(__windows_copy_files YES)

# Any number of files to copy from SOURCE_DIR to DEST_DIR can be specified after DEST_DIR.
# This copying happens post-build.
function(windows_copy_files TARGET SOURCE_DIR DEST_DIR)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEST_DIR})
    foreach(pattern ${ARGN})
        file(GLOB files "${SOURCE_DIR}/${pattern}" FOLLOW_SYMLINKS)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND echo "Globbed ${SOURCE_DIR}/${pattern} to ${files}")
        if (files)
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND echo "Moving ${files} to ${DEST_DIR}")
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy ${files} ${DEST_DIR})
        endif()
    endforeach(pattern)
endfunction()
