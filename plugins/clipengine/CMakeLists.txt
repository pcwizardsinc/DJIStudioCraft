# =============================================================================
#  osv_clipengine - the importer's clip engine as a reusable static library.
#
#  osvtool and Avata 360 Editor (osvgui) both render clips in-process through
#  ImporterInstance.  Rather than each target compiling the same importer
#  sources, they are gathered here once, with OSV_CLIP_ENGINE_WITHOUT_PREMIERE
#  so no Adobe header is ever pulled in - exactly the configuration osvtool
#  used when it compiled these sources itself.
#
#  The result is the SAME frame Premiere and Resolve show for a clip's Source
#  Settings, so osvtool and the editor agree with each other and with the
#  plug-ins.
# =============================================================================

set(_importer "${CMAKE_SOURCE_DIR}/plugins/importer")
set(_common   "${CMAKE_SOURCE_DIR}/plugins/common")

add_library(osv_clipengine STATIC
  "${_importer}/ImporterInstance.cpp"
  "${_importer}/SteadyStage.cpp"
  "${_importer}/SceneLightStage.cpp"
  "${_importer}/FlareStage.cpp"
  "${_importer}/ProtectorGuard.cpp"
  "${_importer}/ImporterAudio.cpp"
  "${_importer}/ImporterGpuFrame.cpp"
  # No direct GPU path outside Premiere (that is the Open 360 Reframe effect's).
  "${_importer}/EngineNoDirect.cpp"
  "${_common}/HostContext.cpp"
  "${_common}/PixelCopy.cpp")

if(WIN32)
  target_sources(osv_clipengine PRIVATE "${_common}/PluginLog.cpp")
else()
  target_sources(osv_clipengine PRIVATE "${_common}/PluginLogPosix.cpp")
endif()

# Consumers get the importer and common headers via this target.
target_include_directories(osv_clipengine PUBLIC "${_importer}" "${_common}")
target_compile_definitions(osv_clipengine PUBLIC OSV_CLIP_ENGINE_WITHOUT_PREMIERE=1)
target_compile_features(osv_clipengine PUBLIC cxx_std_20)
set_target_properties(osv_clipengine PROPERTIES FOLDER "plugins")

# The engine modules the clip engine calls into.  PUBLIC so a consumer that
# links osv_clipengine inherits them and the OSV_HAVE_* defines.
set(_ce_libs osv_core)
foreach(_m container meta color video geom render io)
  if(TARGET osv_${_m})
    list(APPEND _ce_libs osv_${_m})
    string(TOUPPER "${_m}" _M)
    target_compile_definitions(osv_clipengine PUBLIC OSV_HAVE_${_M}=1)
  endif()
endforeach()
target_link_libraries(osv_clipengine PUBLIC ${_ce_libs})

if(TARGET osv_ffmpeg)
  target_link_libraries(osv_clipengine PUBLIC osv_ffmpeg)  # the clip engine decodes its own audio track
endif()
if(WIN32 AND OSV_ENABLE_CUDA)
  target_link_libraries(osv_clipengine PUBLIC CUDA::cuda_driver delayimp)
endif()
