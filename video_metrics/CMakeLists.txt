cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
project(VIDEO_METRICS)
subdirs(iqa)

ADD_DEFINITIONS("-std=c99")
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_VERBOSE_MAKEFILE on)

add_executable(video_metrics video_metrics.c iqa/convolve.c iqa/decimate.c iqa/math_utils.c iqa/ms_ssim.c iqa/mse.c iqa/psnr.c iqa/ssim.c )
target_link_libraries(video_metrics m)

