# Deletes every build directory, the images in build/images included: cmake -P tools/clean.cmake
cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH root)
file(REMOVE_RECURSE "${root}/build" "${root}/utility/build" "${root}/cmake-build-debug")
