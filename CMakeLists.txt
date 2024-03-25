CMAKE_MINIMUM_REQUIRED(VERSION 3.8.0)
set (CMAKE_CXX_STANDARD 17)

PROJECT(vc_optimization CXX)

SET (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# ----- Eigen -----
INCLUDE(FetchContent)
FetchContent_Declare(
    eigen3
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 9441d94dccccd5db8d64179516fdc5b53994a047
)
FetchContent_Populate(eigen3)


# ----- AlgLib -----
FetchContent_Declare(alglib
    URL https://www.alglib.net/translator/re/alglib-3.20.0.cpp.gpl.zip
)
FetchContent_Populate(alglib)
file(GLOB alglib_sources CONFIGURE_DEPENDS ${alglib_SOURCE_DIR}/src/*.cpp)
add_library(alglib STATIC ${alglib_sources})
target_include_directories(alglib PUBLIC ${alglib_SOURCE_DIR}/src)

# ----- Imgui -----
FetchContent_Declare(
	imgui
	URL https://github.com/ocornut/imgui/archive/docking.zip
)
FetchContent_Populate(imgui)

add_library(imgui STATIC 
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_demo.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})

IF(WIN32)
  ADD_DEFINITIONS(-DNOMINMAX)
ENDIF()

SET(VP_SOURCES 
shader_MinGather_LowRes.hlsl
shader_RenderFragments.hlsl
shader_SortFragments_LowRes.hlsl
shader_SortFragments.hlsl
shader_CalculateHistogram.hlsl
shader_ReplaceScalarColor.hlsl
shader_DebugQuad.hlsl
)

SET(VGP_SOURCES 
shader_CreateLists_HQ.hlsl
shader_CreateLists_LowRes.hlsl
shader_DebugGeometry.hlsl
)

SET(CS_SOURCES 
shader_FadeToAlphaPerVertex.hlsl
shader_SmoothAlpha.hlsl
shader_CalculateHistogramCDF.hlsl
shader_CalculateBiHistogramCDF.hlsl
shader_CalculateSegmentedHistogramCDF.hlsl
shader_ExponentialDamping.hlsl
)

SET(HLSLI
    shader_Common.hlsli
    shader_FragmentList_HQ.hlsli
    shader_FragmentList_LowRes.hlsli
    shader_ConstantBuffers.hlsli
    shader_QuadFormat.hlsli
)

FILE(GLOB OBJ_SOURCES_FULL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/data/*.obj")
set(OBJ_SOURCES "")
foreach (entry ${OBJ_SOURCES_FULL_PATH})
    get_filename_component(barename ${entry} NAME)
    get_filename_component(pathname ${entry} DIRECTORY)
    get_filename_component(dir ${pathname} NAME)
    LIST(APPEND OBJ_SOURCES ${dir}/${barename})
endforeach()

FILE(GLOB DISTANCE_SOURCES_FULL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/distanceMatrix/*.dist")
set(DIST_SOURCES "")
foreach (entry ${DISTANCE_SOURCES_FULL_PATH})
    get_filename_component(barename ${entry} NAME)
    get_filename_component(pathname ${entry} DIRECTORY)
    get_filename_component(dir ${pathname} NAME)
    LIST(APPEND DIST_SOURCES ${dir}/${barename})
endforeach()

add_library(eigen INTERFACE)
target_include_directories(eigen INTERFACE ${eigen3_SOURCE_DIR})

# executable
set(SOURCES main.cpp camera.hpp cbuffer.hpp d3d.hpp lines.cpp lines.hpp math.hpp renderer.cpp renderer.hpp rendertarget2d.cpp rendertarget2d.hpp buffer.cpp buffer.hpp shader.cpp shader.hpp imgui_helper.cpp imgui_helper.hpp colormap.cpp colormap.hpp scene.cpp scene.hpp gpuprofiler.cpp gpuprofiler.hpp ${VP_SOURCES} ${VGP_SOURCES} ${CS_SOURCES} ${HLSLI} ${OBJ_SOURCES} ${DIST_SOURCES})
ADD_EXECUTABLE(vc_optimization ${SOURCES})
TARGET_LINK_LIBRARIES(vc_optimization d3d11.lib dxgi.lib eigen imgui alglib)

if(WIN32) # Check if we are on Windows
  if(MSVC) # Check if we are using the Visual Studio compiler
    set_target_properties(vc_optimization PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/$(CONFIGURATION)")
    set_target_properties(vc_optimization PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
  endif()
endif()


foreach(SOURCE ${VP_SOURCES})
  GET_FILENAME_COMPONENT(FILE_WE ${SOURCE} NAME_WE)
  ADD_CUSTOM_COMMAND(OUTPUT ${FILE_WE}.vso
					 COMMAND fxc.exe /Fo ${CMAKE_BINARY_DIR}/$<CONFIGURATION>/${FILE_WE}.vso ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE} /nologo /O2 /Tvs_5_0 /Zi /E VS
					 COMMAND fxc.exe /Fo ${CMAKE_BINARY_DIR}/$<CONFIGURATION>/${FILE_WE}.pso ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE} /nologo /O2 /Tps_5_0 /Zi /E PS
                     MAIN_DEPENDENCY ${SOURCE}
                     COMMENT "Compile vertex and pixel shader: ${SOURCE} \n ${COMMAND}"
                     VERBATIM)
endforeach(SOURCE)

foreach(SOURCE ${VGP_SOURCES})
  GET_FILENAME_COMPONENT(FILE_WE ${SOURCE} NAME_WE)
  ADD_CUSTOM_COMMAND(OUTPUT ${FILE_WE}.vso
					 COMMAND fxc.exe /Fo ${CMAKE_BINARY_DIR}/$<CONFIGURATION>/${FILE_WE}.vso ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE} /nologo /O2 /Tvs_5_0 /Zi /E VS
					 COMMAND fxc.exe /Fo ${CMAKE_BINARY_DIR}/$<CONFIGURATION>/${FILE_WE}.gso ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE} /nologo /O2 /Tgs_5_0 /Zi /E GS
					 COMMAND fxc.exe /Fo ${CMAKE_BINARY_DIR}/$<CONFIGURATION>/${FILE_WE}.pso ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE} /nologo /O2 /Tps_5_0 /Zi /E PS
                     MAIN_DEPENDENCY ${SOURCE}
                     COMMENT "Compile vertex, geometry and pixel shader: ${SOURCE} \n ${COMMAND}"
                     VERBATIM)
endforeach(SOURCE)

foreach(SOURCE ${CS_SOURCES})
  GET_FILENAME_COMPONENT(FILE_WE ${SOURCE} NAME_WE)
  ADD_CUSTOM_COMMAND(OUTPUT ${FILE_WE}.cso
					 COMMAND fxc.exe /Fo ${CMAKE_BINARY_DIR}/$<CONFIGURATION>/${FILE_WE}.cso ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE} /nologo /O2 /Tcs_5_0 /Zi /E CS
                     MAIN_DEPENDENCY ${SOURCE}
                     COMMENT "Compile compute shader: ${SOURCE} \n ${COMMAND}"
                     VERBATIM)
endforeach(SOURCE)

foreach(SOURCE ${OBJ_SOURCES})
  GET_FILENAME_COMPONENT(FILE_WE ${SOURCE} NAME_WE)
  ADD_CUSTOM_COMMAND(OUTPUT ${FILE_WE}.obj
					 COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE} ${CMAKE_BINARY_DIR}/$<CONFIGURATION>/${SOURCE}
                     MAIN_DEPENDENCY ${SOURCE}
                     COMMENT "Copy resource to output: ${SOURCE} \n ${COMMAND}"
                     VERBATIM)
endforeach(SOURCE)

foreach(SOURCE ${DIST_SOURCES})
  GET_FILENAME_COMPONENT(FILE_WE ${SOURCE} NAME_WE)
  ADD_CUSTOM_COMMAND(OUTPUT ${FILE_WE}.dist
					 COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE} ${CMAKE_BINARY_DIR}/$<CONFIGURATION>/${SOURCE}
                     MAIN_DEPENDENCY ${SOURCE}
                     COMMENT "Copy resource to output: ${SOURCE} \n ${COMMAND}"
                     VERBATIM)
endforeach(SOURCE)

set(FILE_URL "https://colorcet.com/download/CETperceptual_csv_0_1.zip")
set(DOWNLOAD_PATH "${CMAKE_CURRENT_BINARY_DIR}/colormap.zip")

file(DOWNLOAD ${FILE_URL} ${DOWNLOAD_PATH}
     SHOW_PROGRESS
     STATUS status)

list(GET status 0 status_code)
if(NOT status_code EQUAL 0)
  message(FATAL_ERROR "Error downloading file: ${status}")
endif()

add_custom_command(TARGET vc_optimization POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E tar xzf ${DOWNLOAD_PATH}
    COMMENT "Unpacking zip ${CMAKE_BINARY_DIR}/$<CONFIGURATION> and ${CMAKE_COMMAND} -E tar xzf ${DOWNLOAD_PATH}"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIGURATION>
    VERBATIM)