add_executable(render
CMakeLists.txt
stdafx.cpp
stdafx.h
TestDriver.cpp
TestScene.cpp
TestScene.h
TestScene0.cpp
TestScene1.cpp
TestScene2.cpp
TestScene3.cpp
TestScene4.cpp
TestScene5.cpp
TestScene6.cpp
TestScene7.cpp
TestScene8.cpp
ViewSettings.h
)

target_link_libraries(render RasterRendererLib)

add_executable(compare_renderers
CMakeLists.txt
stdafx.cpp
stdafx.h
CompareRenderers.cpp
TestScene.cpp
TestScene.h
TestScene0.cpp
TestScene1.cpp
TestScene2.cpp
TestScene3.cpp
TestScene4.cpp
TestScene5.cpp
TestScene6.cpp
TestScene7.cpp
TestScene8.cpp
ViewSettings.h
)

target_link_libraries(compare_renderers RasterRendererLib)

add_executable(compare_renderers_gpu
CMakeLists.txt
stdafx.cpp
stdafx.h
CompareRenderersGPU.cpp
TestScene.cpp
TestScene.h
TestScene0.cpp
TestScene1.cpp
TestScene2.cpp
TestScene3.cpp
TestScene4.cpp
TestScene5.cpp
TestScene6.cpp
TestScene7.cpp
TestScene8.cpp
ViewSettings.h
)

target_link_libraries(compare_renderers_gpu RasterRendererLib)

add_executable(test_forward
CMakeLists.txt
stdafx.cpp
stdafx.h
TestForwardOnly.cpp
TestScene.cpp
TestScene.h
TestScene0.cpp
ViewSettings.h
)

target_link_libraries(test_forward RasterRendererLib)

add_executable(nfl_video_renderer
CMakeLists.txt
stdafx.cpp
stdafx.h
NFLVideoRenderer.cpp
NFLTrackingData.cpp
TestScene.cpp
TestScene.h
ViewSettings.h
)

target_link_libraries(nfl_video_renderer RasterRendererLib)

add_executable(compare_nfl_renderers
CMakeLists.txt
stdafx.cpp
stdafx.h
CompareNFLRenderers.cpp
NFLVideoRenderer.cpp
NFLTrackingData.cpp
TestScene.cpp
TestScene.h
ViewSettings.h
)

# Define NFL_VIDEO_RENDERER_MAIN only for nfl_video_renderer, not for compare_nfl_renderers
set_target_properties(nfl_video_renderer PROPERTIES COMPILE_DEFINITIONS "NFL_VIDEO_RENDERER_MAIN")

target_link_libraries(compare_nfl_renderers RasterRendererLib)