PROJECT(PcieuniDriverTest)
cmake_minimum_required(VERSION 2.8)

ENABLE_TESTING()

IF(NOT Pcieuni_DIR)
  set(Pcieuni_DIR "${CMAKE_SOURCE_DIR}/..")
ENDIF(NOT Pcieuni_DIR)
set(CMAKE_CXX_FLAGS "-D_FILE_OFFSET_BITS=64 -Wall")
  
INCLUDE_DIRECTORIES( ${Pcieuni_DIR} include )
#message("{Pcieuni_DIR} ${Pcieuni_DIR}")

#SET(GCC_FLAGS "-D_FILE_OFFSET_BITS 64")
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_FLAGS}" )
#create a library with the test tools
AUX_SOURCE_DIRECTORY( ${CMAKE_SOURCE_DIR}/src ${PROJECT_NAME}_SOURCES )
add_library(${PROJECT_NAME}_TEST_LIBRARY ${${PROJECT_NAME}_SOURCES} )

#add the executables
aux_source_directory(${CMAKE_SOURCE_DIR}/executables_src testExecutables)
foreach( testExecutableSrcFile ${testExecutables})
  #NAME_WE means the base name without path and (longest) extension
  get_filename_component(excutableName ${testExecutableSrcFile} NAME_WE)
  add_executable(${excutableName} ${testExecutableSrcFile})
  target_link_libraries(${excutableName} ${PROJECT_NAME}_TEST_LIBRARY)    
  add_test(${excutableName} ${excutableName})
endforeach( testExecutableSrcFile )


