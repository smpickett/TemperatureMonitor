@echo off

rem -- get the device id
if not exist user.deviceid.txt (
  echo You must enter your lengthy device ID into a file named 'user.deviceid.txt'
  pause
  exit /b 1
  )
set /p DEVICE_ID=<user.deviceid.txt

rem -- find the last bin made by the compiler
for /f %%s in ('dir *.bin /B /O:-D') do set newbin=%%s & goto DONE
:DONE

rem -- flash the photon
particle flash %DEVICE_ID% %newbin%
pause

