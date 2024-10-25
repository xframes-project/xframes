cd ..\deps/glfw
cmake -DBUILD_SHARED_LIBS=ON -DUSE_MSVC_RUNTIME_LIBRARY_DLL=ON -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF .
cd ..\..
msbuild /p:Platform=x64 /p:Configuration=Release /p:PlatformToolset=v142 ..\deps\glfw\ALL_BUILD.vcxproj
copy /y ..\deps\glfw\src\Release\glfw3.dll .\
copy /y ..\deps\glfw\src\Release\glfw3dll.lib .\
