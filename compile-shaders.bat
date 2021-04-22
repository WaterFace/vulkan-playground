@echo off

cd shaders

for /f "usebackq" %%v in (`dir /b /a-d ^| findstr /v /i "\.spv$"`) do (
    echo Compiling %%v...
    %VULKAN_SDK%/bin32/glslc.exe "%%v" -o "%%v.spv"
  )

cd ..
