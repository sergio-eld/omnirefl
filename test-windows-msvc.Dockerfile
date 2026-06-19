# escape=`

FROM mcr.microsoft.com/windows/servercore:ltsc2019 AS build

SHELL ["cmd", "/S", "/C"]

# Windows package-test image for MSVC-compatible toolchains.

RUN curl -SL --output dotnet-framework-installer.exe https://download.visualstudio.microsoft.com/download/pr/2d6bb6b2-226a-4baa-bdec-798822606ff1/8494001c276a4b96804cde7829c04d7f/ndp48-x86-x64-allos-enu.exe `
    && start /w dotnet-framework-installer.exe /q /norestart `
    && del dotnet-framework-installer.exe

RUN powershell -NoProfile -Command `
    Set-ExecutionPolicy Bypass -Scope Process -Force; `
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; `
    iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))

RUN choco install -y ninja cmake

RUN powershell -NoProfile -Command "Remove-Item -Recurse -Force @($env:TEMP, 'C:\Windows\Temp', 'C:\ProgramData\chocolatey\cache') -ErrorAction SilentlyContinue; New-Item -ItemType Directory -Force -Path @($env:TEMP, 'C:\Windows\Temp') | Out-Null; exit 0"

RUN mkdir C:\Temp `
    && cd /d C:\Temp `
    && curl -SL --output vs_buildtools.exe https://aka.ms/vs/17/release/vs_buildtools.exe `
    && powershell -NoProfile -Command "$vs_args = @('--quiet', '--wait', '--norestart', '--nocache', '--installPath', 'C:\BuildTools', '--add', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '--add', 'Microsoft.VisualStudio.Component.VC.Redist.14.Latest', '--add', 'Microsoft.VisualStudio.Component.Windows10SDK', '--add', 'Microsoft.VisualStudio.Component.Windows10SDK.19041'); $p = Start-Process -FilePath .\vs_buildtools.exe -ArgumentList $vs_args -Wait -PassThru; if ((0 -ne $p.ExitCode) -and (3010 -ne $p.ExitCode)) { exit $p.ExitCode }" `
    && del /q C:\Temp\vs_buildtools.exe `
    && powershell -NoProfile -Command "Remove-Item -Recurse -Force @($env:TEMP, 'C:\Windows\Temp') -ErrorAction SilentlyContinue; New-Item -ItemType Directory -Force -Path @($env:TEMP, 'C:\Windows\Temp') | Out-Null; exit 0"

RUN choco install -y git `
    && powershell -NoProfile -Command "Remove-Item -Recurse -Force @($env:TEMP, 'C:\Windows\Temp', 'C:\ProgramData\chocolatey\cache') -ErrorAction SilentlyContinue; New-Item -ItemType Directory -Force -Path @($env:TEMP, 'C:\Windows\Temp') | Out-Null; exit 0"

ARG TOOLCHAIN=msvc

# clang-cl needs MSVC headers/libs from Build Tools, but the Visual Studio LLVM
# component is much larger than needed for package tests. Install standalone
# LLVM for clang-cl and keep the MSVC image unchanged.
RUN if "%TOOLCHAIN%"=="clang-cl" ( `
        choco install -y llvm `
        && powershell -NoProfile -Command "$keep = @('clang-cl.exe', 'clang.exe', 'clang++.exe', 'lld-link.exe'); Get-ChildItem 'C:\Program Files\LLVM\bin' -File -Filter '*.exe' | Where-Object { $keep -notcontains $_.Name } | Remove-Item -Force" `
        && powershell -NoProfile -Command "Remove-Item -Recurse -Force @($env:TEMP, 'C:\Windows\Temp', 'C:\ProgramData\chocolatey\cache') -ErrorAction SilentlyContinue; New-Item -ItemType Directory -Force -Path @($env:TEMP, 'C:\Windows\Temp') | Out-Null; exit 0" `
    ) else if not "%TOOLCHAIN%"=="msvc" ( `
        echo Unsupported TOOLCHAIN='%TOOLCHAIN%' `
        && exit /b 1 `
    )

ENTRYPOINT ["cmd", "/S", "/C"]
