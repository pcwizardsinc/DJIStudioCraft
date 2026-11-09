# =============================================================================
#  OsvImGui.cmake
#
#  Dear ImGui for OpenOSV Studio (tools/osvgui) on systems without vcpkg.
#
#  vcpkg builds imgui with its GLFW + OpenGL 3 backends and exports
#  imgui::imgui.  Linux distributions either do not package Dear ImGui or
#  package it without those backends, so on Linux the same release vcpkg pins
#  (v1.92.9b) is compiled here, from one of:
#
#    OSV_IMGUI_SOURCE_DIR  an unpacked imgui-1.92.9b tree (offline / RPM
#                          builds: ship the tarball as a second Source and
#                          point this at it)
#    otherwise             the release tarball, downloaded at configure time
#                          and checked against its SHA-256.
#
#  Result: the static target imgui::imgui with the include layout the app's
#  sources expect (<imgui.h>, <imgui_impl_glfw.h>, <imgui_impl_opengl3.h>,
#  <imgui_stdlib.h>).
# =============================================================================
if(TARGET imgui::imgui)
  return()
endif()

set(OSV_IMGUI_VERSION "1.92.9b")
set(OSV_IMGUI_SHA256 "21d8a0a565e85dce943e375db00812c2f3f0ab21f3f0f7964e364a63422d7f99")
set(OSV_IMGUI_SOURCE_DIR "" CACHE PATH "Unpacked Dear ImGui ${OSV_IMGUI_VERSION} source tree (skips the download)")

if(OSV_IMGUI_SOURCE_DIR)
  set(_imgui_dir "${OSV_IMGUI_SOURCE_DIR}")
else()
  include(FetchContent)
  FetchContent_Declare(osv_imgui
    URL "https://github.com/ocornut/imgui/archive/refs/tags/v${OSV_IMGUI_VERSION}.tar.gz"
    URL_HASH "SHA256=${OSV_IMGUI_SHA256}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(osv_imgui)
  set(_imgui_dir "${osv_imgui_SOURCE_DIR}")
endif()

if(NOT EXISTS "${_imgui_dir}/imgui.cpp" OR NOT EXISTS "${_imgui_dir}/backends/imgui_impl_glfw.cpp")
  message(FATAL_ERROR "Dear ImGui sources not found in '${_imgui_dir}'")
endif()

add_library(osv_imgui STATIC
  "${_imgui_dir}/imgui.cpp"
  "${_imgui_dir}/imgui_draw.cpp"
  "${_imgui_dir}/imgui_tables.cpp"
  "${_imgui_dir}/imgui_widgets.cpp"
  "${_imgui_dir}/imgui_demo.cpp"
  "${_imgui_dir}/backends/imgui_impl_glfw.cpp"
  "${_imgui_dir}/backends/imgui_impl_opengl3.cpp"
  "${_imgui_dir}/misc/cpp/imgui_stdlib.cpp")
target_include_directories(osv_imgui SYSTEM PUBLIC
  "${_imgui_dir}"
  "${_imgui_dir}/backends"
  "${_imgui_dir}/misc/cpp")
target_link_libraries(osv_imgui PUBLIC glfw OpenGL::GL ${CMAKE_DL_LIBS})
set_target_properties(osv_imgui PROPERTIES FOLDER "third_party" POSITION_INDEPENDENT_CODE ON)
add_library(imgui::imgui ALIAS osv_imgui)

message(STATUS "Dear ImGui     : ${OSV_IMGUI_VERSION} built from ${_imgui_dir}")
