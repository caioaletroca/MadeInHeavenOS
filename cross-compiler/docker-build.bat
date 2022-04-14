@REM Copy sysroot to container
robocopy ../sysroot ./sysroot /E

@REM Build docker image
docker build . -t mihos-compiler