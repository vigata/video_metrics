cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
project(VIDEO_METRICS)
subdirs(iqa)

set(CMAKE_BUILD_TYPE Release)

add_executable(video_metrics video_metrics.c iqa/convolve.c iqa/decimate.c iqa/math_utils.c iqa/ms_ssim.c iqa/mse.c iqa/psnr.c iqa/ssim.c )

