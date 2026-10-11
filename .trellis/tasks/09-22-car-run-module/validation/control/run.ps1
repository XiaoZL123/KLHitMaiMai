$ErrorActionPreference = 'Stop'
$taskProject = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..\..')).Path
$controlCompiler = 'C:\mingw64\bin\gcc.exe'
$controlIncludes = Join-Path $taskProject 'SoftWare\BIGDICK\Core\Inc'
$controlSource = Join-Path $taskProject 'SoftWare\BIGDICK\Core\Src\car_control.c'
$controlTest = Join-Path $PSScriptRoot 'control_test.c'
$controlOutput = Join-Path $env:TEMP 'gongchuang-car-control-test.exe'
$controlInstallOutput = Join-Path $env:TEMP 'gongchuang-car-control-install-test.exe'

& $controlCompiler -std=c11 -Wall -Wextra -Werror -I $PSScriptRoot -I $controlIncludes $controlSource $controlTest -lm -o $controlOutput
if ($LASTEXITCODE -ne 0) { throw '默认控制模拟编译失败。' }
& $controlOutput
if ($LASTEXITCODE -ne 0) { throw '默认控制模拟断言失败。' }

& $controlCompiler -std=c11 -Wall -Wextra -Werror -I $PSScriptRoot -I $controlIncludes '-DCONTROL_INSTALL_TEST=1' '-DCAR_CONTROL_OPS_INSTALL_YAW_DEG=90.0F' '-DCAR_CONTROL_OPS_OFFSET_FORWARD_MM=100.0F' '-DCAR_CONTROL_OPS_OFFSET_LEFT_MM=50.0F' '-DCAR_CONTROL_OPS_HEADING_SIGN=-1.0F' $controlSource $controlTest -lm -o $controlInstallOutput
if ($LASTEXITCODE -ne 0) { throw '安装配置模拟编译失败。' }
& $controlInstallOutput
if ($LASTEXITCODE -ne 0) { throw '安装配置模拟断言失败。' }
