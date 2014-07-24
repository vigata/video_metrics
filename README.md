video_metrics
=========

video_metrics is a simple tool for video metric calculation. It uses the [Image Quality Assessment (IQA)] library for metric calculation. IQA is fast and accurate. [codecbench] uses it as its main tool for video metric calculation 

##Build##
Go into video_metrics folder and type
```
cmake CMakeLists.txt
```
this should generate a 'video_metrics' binary after compiling

##Usage##
```
video_metrics -a ref.yuv -b rec.yuv -m [psnr|ssim|ms_ssim|psnr,ssim|psnr,ssim,ms_ssim|...] -w width -h height -f format

Available metrics:
psnr
ssim
ms_ssim
mse
```
##Author##
Check my other projects like [codecbench], [CABSscore specification], or my very own [linked in profile](http://www.linkedin.com/in/vigata)


[CABSscore specification]:http://codecbench.nelalabs.com/cabs
[codecbench]:http://github.com/concalma/codecbench
[alberto vigata]:http://www.linkedin.com/in/vigata
[Image Quality Assessment (IQA)]:http://tdistler.com/iqa/
