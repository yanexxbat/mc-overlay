<#
.SYNOPSIS
    Configures and builds mcov (overlay + agent).

.DESCRIPTION
    Wraps the CMake invocation so the JDK path and build type do not have to be
    retyped. Every path is quoted because this tree normally lives under a
    directory containing a space.

.EXAMPLE
    .\scripts\build.ps1
    .\scripts\build.ps1 -Config Debug -Clean
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string] $Config = 'Release',

    [string] $JavaHome = $env:JAVA_HOME,

    [string] $Generator = '',

    [switch] $Clean
)

$ErrorActionPreference = 'Stop'

$RepoRoot  = Split-Path -Parent $PSScriptRoot
$BuildDir  = Join-Path $RepoRoot 'build'

Write-Host "mcov build" -ForegroundColor Cyan
Write-Host "  root  : $RepoRoot"
Write-Host "  config: $Config"

function Get-BuildJavaMajor([string] $JdkRoot) {
    $releaseFile = Join-Path $JdkRoot 'release'
    if (-not (Test-Path -LiteralPath $releaseFile)) { return 0 }
    foreach ($releaseLine in Get-Content -LiteralPath $releaseFile) {
        if ($releaseLine -match '^JAVA_VERSION="?(\d+)') { return [int] $Matches[1] }
    }
    return 0
}

# --- locate a build JDK 17+ --------------------------------------------------
if (-not $JavaHome) {
    $candidates = @(
        'C:\Program Files\Eclipse Adoptium',
        'C:\Program Files\Java',
        'C:\Program Files\Microsoft'
    ) | Where-Object { Test-Path $_ }

    foreach ($root in $candidates) {
        $match = Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue |
                 Where-Object { (Get-BuildJavaMajor $_.FullName) -ge 17 -and
                                (Test-Path (Join-Path $_.FullName 'bin\javac.exe')) } |
                 Sort-Object Name -Descending |
                 Select-Object -First 1
        if ($match) { $JavaHome = $match.FullName; break }
    }
}

if (-not $JavaHome -or -not (Test-Path (Join-Path $JavaHome 'include\jni.h')) -or
    (Get-BuildJavaMajor $JavaHome) -lt 17) {
    throw "No JDK 17+ found. Pass -JavaHome '<path to a full JDK>' or set JAVA_HOME. " +
          "A JRE will not work: the agent needs jvmti.h and the loader needs javac."
}

Write-Host "  jdk   : $JavaHome"

# --- locate the toolchain ----------------------------------------------------
# Visual Studio Build Tools ships CMake and Ninja, but does not put them (or the
# MSVC compiler) on PATH. Rather than demanding a separate CMake install, find
# the VS installation with vswhere and run the build inside its vcvars
# environment.
$VcVars = $null
$CMakeExe = (Get-Command cmake -ErrorAction SilentlyContinue).Source

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path $vswhere) {
    $vsRoot = & $vswhere -latest -products * `
                         -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                         -property installationPath 2>$null | Select-Object -First 1

    if ($vsRoot) {
        Write-Host "  vs    : $vsRoot"
        $candidate = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
        if (Test-Path $candidate) { $VcVars = $candidate }

        if (-not $CMakeExe) {
            $bundled = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path $bundled) {
                $CMakeExe = $bundled
                $env:PATH = (Split-Path $bundled) + ';' +
                            (Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja') + ';' +
                            $env:PATH
            }
        }
    }
}

if (-not $CMakeExe) {
    throw "cmake not found, on PATH or inside a Visual Studio installation. Install " +
          "CMake 3.20+ and the Visual Studio Build Tools (Desktop development with C++)."
}
Write-Host "  cmake : $CMakeExe"

# Runs a cmake invocation, wrapped in vcvars64 when MSVC is not already active.
function Invoke-CMake {
    param([string[]] $Arguments)

    # Out-Host on every invocation below is load-bearing: a PowerShell function
    # returns everything written to the output stream, so without it the caller
    # receives cmake's console output *plus* the exit code as an array - and
    # "-ne 0" on an array filters instead of comparing, which reads as failure
    # no matter what cmake actually did.
    if (-not $VcVars -or (Get-Command cl -ErrorAction SilentlyContinue)) {
        & $CMakeExe @Arguments | Out-Host
        return $LASTEXITCODE
    }

    # cmd.exe is the only thing that can source vcvars64.bat into its own
    # environment. Passing the whole "call ... && cmake ..." line as a single
    # /c argument does not survive PowerShell's and cmd's combined quote
    # handling, so write a real script file and run that instead.
    $quoted = ($Arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $script = Join-Path ([System.IO.Path]::GetTempPath()) ("mcov_build_" + [guid]::NewGuid().ToString('N') + ".cmd")

    @(
        '@echo off'
        # 2>&1 matters: vcvars64.bat writes harmless "vswhere.exe not found"
        # noise to stderr, and PowerShell turns any native stderr line into a
        # NativeCommandError that trips $ErrorActionPreference = 'Stop'.
        'call "{0}" >nul 2>&1' -f $VcVars
        'if errorlevel 1 exit /b 90'
        '"{0}" {1}' -f $CMakeExe, $quoted
        'exit /b %errorlevel%'
    ) | Set-Content -Path $script -Encoding ASCII

    try {
        & cmd.exe /c $script | Out-Host
        return $LASTEXITCODE
    }
    finally {
        Remove-Item $script -ErrorAction SilentlyContinue
    }
}

# --- is the agent currently loaded in a live JVM? ----------------------------
# Windows will not let the linker overwrite a DLL that is mapped into a running
# process, and the raw failure is an opaque "LNK1104: cannot open file". Since
# the whole point of this DLL is to live inside Minecraft, hitting this while
# the game is open is the normal case rather than an edge case.
$agentDll = Join-Path $BuildDir 'bin\mcov_agent.dll'
if (Test-Path $agentDll) {
    $holders = @()
    foreach ($p in Get-Process java, javaw -ErrorAction SilentlyContinue) {
        try {
            if ($p.Modules | Where-Object { $_.FileName -eq $agentDll }) {
                $holders += "$($p.ProcessName) (pid $($p.Id))"
            }
        } catch {
            # A process we cannot introspect is not necessarily a holder; ignore.
        }
    }
    if ($holders.Count -gt 0) {
        Write-Host ""
        Write-Host "  The agent DLL is loaded into a running JVM:" -ForegroundColor Yellow
        $holders | ForEach-Object { Write-Host "    $_" -ForegroundColor Yellow }
        Write-Host ""
        Write-Host "  Windows cannot overwrite a mapped DLL, so the link would fail with" -ForegroundColor Yellow
        Write-Host "  LNK1104. Close Minecraft and run this again." -ForegroundColor Yellow
        Write-Host ""
        Write-Host "  (Unloading the agent from the menu is not enough - the JVM keeps the" -ForegroundColor DarkGray
        Write-Host "   library mapped until the process exits.)" -ForegroundColor DarkGray
        throw "agent DLL is in use by a running JVM"
    }
}

# --- configure ---------------------------------------------------------------
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "  cleaning $BuildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

$configureArgs = @(
    '-S', $RepoRoot
    '-B', $BuildDir
    "-DMCOV_JAVA_HOME=$JavaHome"
    "-DCMAKE_BUILD_TYPE=$Config"
)

if ($Generator) {
    $configureArgs += @('-G', $Generator)
}
elseif ($VcVars) {
    # Inside vcvars the compiler is already selected, so Ninja is faster than the
    # VS generator and needs no -A. Bitness comes from vcvars64 itself - which
    # matters, because a 32-bit agent cannot be loaded into a 64-bit JVM.
    $configureArgs += @('-G', 'Ninja')
}
else {
    $configureArgs += @('-A', 'x64')
}

Write-Host "`n> cmake $($configureArgs -join ' ')" -ForegroundColor DarkGray
if ((Invoke-CMake $configureArgs) -ne 0) { throw "cmake configure failed" }

# --- build -------------------------------------------------------------------
$buildArgs = @('--build', $BuildDir, '--parallel')
if (-not $VcVars -or $Generator) { $buildArgs += @('--config', $Config) }

Write-Host "`n> cmake $($buildArgs -join ' ')" -ForegroundColor DarkGray
if ((Invoke-CMake $buildArgs) -ne 0) { throw "build failed" }

# --- report ------------------------------------------------------------------
$binDir = Join-Path $BuildDir 'bin'
if (Test-Path (Join-Path $binDir $Config)) { $binDir = Join-Path $binDir $Config }

Write-Host "`nArtifacts:" -ForegroundColor Green
Get-ChildItem -Path $binDir -Include 'mcov_overlay.exe', 'mcov_injector.exe', 'mcov_agent.dll' -Recurse -ErrorAction SilentlyContinue |
    ForEach-Object { Write-Host ("  {0,-22} {1,10:N0} bytes" -f $_.Name, $_.Length) }

foreach ($helperName in @('AgentLoader.class', 'frame-sync-agent.jar', 'frame-signal.jar')) {
    $helperPath = Join-Path $binDir "tools\$helperName"
    if (Test-Path -LiteralPath $helperPath) {
        Write-Host ("  {0,-22} (attach/frame helper)" -f $helperName)
    } else {
        Write-Host "  $helperName MISSING - rebuild the complete project with a full JDK." -ForegroundColor Yellow
    }
}

Write-Host "`nRun:  $binDir\mcov_overlay.exe" -ForegroundColor Cyan
