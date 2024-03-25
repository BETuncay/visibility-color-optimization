cmake_minimum_required(VERSION 3.18)

PROJECT(transformer CXX C)

set(CMAKE_CXX_STANDARD 17)

IF(WIN32)
  ADD_DEFINITIONS(-DNOMINMAX)
  add_compile_options(-bigobj)
  add_compile_options(-MP)
ENDIF()

include(FetchContent)

# ----- Nanoflann -----
FetchContent_Populate(nanoflann
    GIT_REPOSITORY https://github.com/jlblancoc/nanoflann
    GIT_TAG v1.5.3
)
add_library(nanoflann INTERFACE)
target_include_directories(nanoflann INTERFACE ${nanoflann_SOURCE_DIR})


# ----- Eigen -----
FetchContent_Declare(eigen3
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG 9441d94dccccd5db8d64179516fdc5b53994a047
        )
FetchContent_Populate(eigen3)
add_library(eigen INTERFACE)
target_include_directories(eigen INTERFACE ${eigen3_SOURCE_DIR})

# ----- AlgLib -----
FetchContent_Declare(alglib
    URL https://www.alglib.net/translator/re/alglib-3.20.0.cpp.gpl.zip
)
FetchContent_Populate(alglib)
file(GLOB alglib_sources CONFIGURE_DEPENDS ${alglib_SOURCE_DIR}/src/*.cpp)
add_library(alglib STATIC ${alglib_sources})
target_include_directories(alglib PUBLIC ${alglib_SOURCE_DIR}/src)

# ----- OpenMP -----
FIND_PACKAGE(OpenMP)
IF(OPENMP_FOUND)
  MESSAGE(STATUS "Using OpenMP parallelization")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
ELSE()
   MESSAGE(STATUS "Not using OpenMP parallelization")
ENDIF()

# ----- VTK -----
find_package(VTK REQUIRED)
include_directories(SYSTEM ${VTK_INCLUDE_DIRS})

SET(AM_SOURCES 
raw_data/benzene.am
raw_data/borromean.am
raw_data/delta65_high.am
raw_data/ECMWF_3D_Reanalysis_Velocity_T0.am
raw_data/trefoil10.am
raw_data/trefoil140.am
raw_data/UCLA_CTBL_Velocity_T6.am
)

# executable
set(SOURCES main.cpp AmiraReader.cpp AmiraReader.hpp AmiraWriter.cpp AmiraWriter.hpp Vorticity.cpp Vorticity.hpp SteadyTracer.cpp SteadyTracer.hpp Sampling.cpp Sampling.hpp ObjectWriter.cpp ObjectWriter.hpp ObjectReader.cpp ObjectReader.hpp LineValues.cpp LineValues.hpp Acceleration.cpp Acceleration.hpp Normalize.cpp Normalize.hpp LineDistanceMetrics.cpp LineDistanceMetrics.hpp ${AM_SOURCES})
ADD_EXECUTABLE(transformer ${SOURCES})
TARGET_LINK_LIBRARIES(transformer eigen alglib nanoflann ${VTK_LIBRARIES})

if(WIN32) # Check if we are on Windows
  if(MSVC) # Check if we are using the Visual Studio compiler
    set_target_properties(transformer PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/$(CONFIGURATION)")
    set_target_properties(transformer PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE") 
  endif()
endif()

vtk_module_autoinit(
  TARGETS transformer
  MODULES ${VTK_LIBRARIES}
)

foreach(SOURCE ${AM_SOURCES})
  GET_FILENAME_COMPONENT(FILE_WE ${SOURCE} NAME_WE)
  ADD_CUSTOM_COMMAND(OUTPUT ${FILE_WE}.obj
					 COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE} ${CMAKE_BINARY_DIR}/$<CONFIGURATION>/${SOURCE}
                     MAIN_DEPENDENCY ${SOURCE}
                     COMMENT "Copy resource to output: ${SOURCE} \n ${COMMAND}"
                     VERBATIM)
endforeach(SOURCE)