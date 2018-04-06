execute_process(COMMAND ${PROJECT_SOURCE_DIR}/get_git_timestamp.sh OUTPUT_VARIABLE GIT_TIMESTAMP)
string(REPLACE "\n" ""  GIT_TIMESTAMP ${GIT_TIMESTAMP} )
message(STATUS "Current git timestamp is ${GIT_TIMESTAMP}\n")
set(CPACK_PACKAGE_VERSION ${GIT_TIMESTAMP})
