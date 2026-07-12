# escape=`

FROM mcr.microsoft.com/windows/servercore:ltsc2019

SHELL ["cmd", "/S", "/C"]

# MSYS2 does not need Visual Studio Build Tools, Chocolatey, or .NET. Nano
# Server is smaller, but MSYS2 bash requires netapi32.dll, which Nano Server
# LTSC2019 does not provide. Server Core LTSC2019 is the smallest verified base
# compatible with this Windows 10 container setup.
ADD https://github.com/msys2/msys2-installer/releases/download/nightly-x86_64/msys2-base-x86_64-latest.sfx.exe C:/msys2.exe

RUN C:\msys2.exe -y -oC:\tools `
    && del C:\msys2.exe `
    && C:\tools\msys64\usr\bin\bash.exe -lc "pacman -Syuu --noconfirm" `
    || C:\tools\msys64\usr\bin\bash.exe -lc "pacman -Syuu --noconfirm"

RUN C:\tools\msys64\usr\bin\bash.exe -lc "pacman -S --noconfirm --needed git unzip mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gcc mingw-w64-clang-x86_64-cmake mingw-w64-clang-x86_64-ninja mingw-w64-clang-x86_64-clang && pacman -Scc --noconfirm"
