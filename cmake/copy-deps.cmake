cmake_minimum_required(VERSION 3.25)

# Print received arguments for debugging
message("Source directory: ${CMAKE_ARGV3}")
message("Destination directory: ${CMAKE_ARGV4}")

# The source and destination directories should be passed as command line arguments
set(SOURCE_DIR "${CMAKE_ARGV3}")
set(DEST_DIR "${CMAKE_ARGV4}")

# Create the destination directory if it does not exist
file(MAKE_DIRECTORY ${DEST_DIR}/files)
file(MAKE_DIRECTORY ${DEST_DIR}/includes)

# Find all DLL files in the source directory, including subdirectories
file(GLOB_RECURSE DLL_FILES "${SOURCE_DIR}/*.dll")
file(GLOB_RECURSE SO_FILES "${SOURCE_DIR}/*.so")
file(GLOB_RECURSE LIB_FILES "${SOURCE_DIR}/*.lib")
file(GLOB_RECURSE HEADER_FILES "${SOURCE_DIR}/*.h")

# Copy each DLL file to the destination directory
foreach(DLL_FILE IN LISTS DLL_FILES)
    message("Copy ${DLL_FILE} to ${DEST_DIR}/files")
    file(COPY ${DLL_FILE} DESTINATION ${DEST_DIR}/files)
endforeach()
foreach(SO_FILE IN LISTS SO_FILES)
    message("Copy ${SO_FILE} to ${DEST_DIR}/files")
    file(COPY ${SO_FILE} DESTINATION ${DEST_DIR}/files)
    file(COPY ${SO_FILE} DESTINATION ${DEST_DIR}/../files)
endforeach()
foreach(LIB_FILE IN LISTS LIB_FILES)
    message("Copy ${LIB_FILE} to ${DEST_DIR}/files")
    file(COPY ${LIB_FILE} DESTINATION ${DEST_DIR}/files)
endforeach()
#foreach(HEADER_FILE IN LISTS HEADER_FILES)
#    message("Copy ${HEADER_FILE} to ${DEST_DIR}/includes")
#    file(COPY ${HEADER_FILE} DESTINATION ${DEST_DIR}/includes)
#    file(COPY ${HEADER_FILE} DESTINATION ${DEST_DIR}/../includes)
#endforeach()