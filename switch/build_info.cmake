# Writes usm_build_info.h with the commit the build was made from. Runs on
# every build (custom target usm_build_info), not at configure time: a
# configure-time `git describe` goes stale as soon as you commit and rebuild
# without re-running cmake. The header is only rewritten when its content
# changes, so an unchanged commit does not force options_menu.c to recompile.
#   cmake -DSRC_DIR=<repo> -DOUT=<header> -P build_info.cmake
set(SHA "")
set(BRANCH "")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} -c safe.directory=* -C ${SRC_DIR} rev-parse --short=7 HEAD
                    OUTPUT_VARIABLE SHA OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} -c safe.directory=* -C ${SRC_DIR} rev-parse --abbrev-ref HEAD
                    OUTPUT_VARIABLE BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} -c safe.directory=* -C ${SRC_DIR} status --porcelain --untracked-files=no
                    OUTPUT_VARIABLE DIRTY OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(SHA AND DIRTY)
        set(SHA "${SHA}-dirty")
    endif()
endif()
if(SHA)
    set(CONTENT "#define USM_BUILD_SHA \"${SHA}\"\n#define USM_BUILD_BRANCH \"${BRANCH}\"\n")
else()
    # No git (or no .git): leave the defines out; the Build row falls back to the compile date.
    set(CONTENT "/* no git information at build time */\n")
endif()
set(OLD "")
if(EXISTS ${OUT})
    file(READ ${OUT} OLD)
endif()
if(NOT OLD STREQUAL CONTENT)
    file(WRITE ${OUT} "${CONTENT}")
endif()
