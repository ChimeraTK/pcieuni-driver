project("pcieuni-driver")
cmake_minimum_required(VERSION 3.5)

# Find all potential kernel modules
file(GLOB MODULES LIST_DIRECTORIES true /lib/modules/*/build)

# Sort and reverse list to have the latest version as first 
list(SORT MODULES)
list(GET MODULES -1 NEWEST_MODULE_PATH)

# Convert that path into a list: /lib/modules/5.0.0/build -> ;lib;modules;5.0.0;build
string(REPLACE "/" ";" NEWEST_MODULE_PATH ${NEWEST_MODULE_PATH})

# The version is on the 4th index:
list(GET NEWEST_MODULE_PATH 3 NEWEST_MODULE_VERSION)

message("Building kernel module for ${NEWEST_MODULE_VERSION}")

add_custom_target(kmod ALL make "KVERSION=${NEWEST_MODULE_VERSION}" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
install(CODE "MESSAGE(\"No install\")")
enable_testing()
add_test(NAME dummy COMMAND true)
