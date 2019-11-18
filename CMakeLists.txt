cmake_minimum_required(VERSION 3.13)

add_subdirectory(apriltag)
add_subdirectory(hawkeye)
target_link_libraries(hawkeye libapriltag.so)
