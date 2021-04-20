@echo off

cd shaders

for /f "usebackq" %%v in (`dir /b /a-d ^| findstr /v /i "\.spv$"`) do (
    %VULKAN_SDK%/bin32/glslc.exe "%%v" -o "%%v.spv"
  )

cd ..
