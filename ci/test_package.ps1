# Run in PowerShell 7 with the Windows C++ toolchain environment initialized.
param(
  [Parameter(Mandatory)] [string] $Package,
  [Parameter(Mandatory)] [string] $Name,
  [Parameter(Mandatory)] [string] $Results,
  [string] $CMakeArg,
  [switch] $SkipExampleRun
)

$ErrorActionPreference = 'Stop'
$archive = Get-Item $Package
$null = mkdir -Force $Results
$Results = (Resolve-Path $Results).Path
# Keep the scratch root short: nested Git submodules hit Windows path limits.
$work = Join-Path ([IO.Path]::GetTempPath()) ([IO.Path]::GetRandomFileName())
$source = "$work/with space"
$install = "$source/install"
$build = "$source/build"
$exampleBuild = "$source/example-build"
$extract = "$work/extract"

try {
  $null = mkdir -Force $source, $extract
  if ($archive.Name.EndsWith('.zip')) {
    Expand-Archive -Path $archive.FullName -DestinationPath $extract
  } elseif ($archive.Name.EndsWith('.tar.gz')) {
    tar -xzf $archive.FullName -C $extract
    if (0 -ne $LASTEXITCODE) { exit $LASTEXITCODE }
  } else {
    throw "unsupported package: $($archive.Name)"
  }

  $roots = @(ls -Path $extract -Directory -Filter 'omnirefl-*')
  if (1 -ne $roots.Count) {
    throw "expected one package root in $($archive.Name)"
  }

  mv $roots[0].FullName $install
  cp -r "$install/share/omnirefl/*" $source
  & "$install/bin/omnirefl.exe" --help 2>&1 |
    tee "$Results/omnirefl-help.log"
  if (0 -ne $LASTEXITCODE) { exit $LASTEXITCODE }
  & "$install/bin/ccdb_query.exe" --help 2>&1 |
    tee "$Results/ccdb-query-help.log"
  if (0 -ne $LASTEXITCODE) { exit $LASTEXITCODE }

  $cmakeArgs = @('-GNinja', '-DCMAKE_BUILD_TYPE=Release',
    "-Domnirefl_DIR=$install/lib/cmake/omnirefl")
  if ($CMakeArg) { $cmakeArgs += $CMakeArg }

  cmake -S "$source/examples" -B $exampleBuild @cmakeArgs 2>&1 |
    tee "$Results/example-configure.log"
  if (0 -ne $LASTEXITCODE) {
    echo 'omnirefl: packaged example configuration failed'
    exit $LASTEXITCODE
  }

  cmake --build $exampleBuild --parallel ([Environment]::ProcessorCount) 2>&1 |
    tee "$Results/example-build.log"
  if (0 -ne $LASTEXITCODE) {
    echo 'omnirefl: packaged example build failed'
    exit $LASTEXITCODE
  }

  if (!$SkipExampleRun) {
    ctest --test-dir $exampleBuild --no-tests=error --timeout 600 `
      --output-junit "$Results/$Name-examples.xml" -V 2>&1 |
      tee "$Results/example-test.log"
    if (0 -ne $LASTEXITCODE) {
      echo 'omnirefl: packaged example tests failed'
      exit $LASTEXITCODE
    }
  }

  cmake -S "$source/tests" -B $build @cmakeArgs 2>&1 |
    tee "$Results/configure.log"
  if (0 -ne $LASTEXITCODE) { exit $LASTEXITCODE }

  cmake --build $build --parallel ([Environment]::ProcessorCount) 2>&1 |
    tee "$Results/build.log"
  if (0 -ne $LASTEXITCODE) { exit $LASTEXITCODE }

  ctest --test-dir $build --no-tests=error --timeout 600 `
    --output-junit "$Results/$Name.xml" -V 2>&1 |
    tee "$Results/$Name.log"
  exit $LASTEXITCODE
} finally {
  $diagnostics = "$Results/diagnostics"
  $null = mkdir -Force $diagnostics
  foreach ($file in @('CMakeCache.txt', 'compile_commands.json',
      'Testing/Temporary/LastTest.log', 'CMakeFiles/CMakeError.log')) {
    $path = "$build/$file"
    if (Test-Path $path) { cp $path $diagnostics }
  }

  $exampleLog = "$exampleBuild/Testing/Temporary/LastTest.log"
  if (Test-Path $exampleLog) {
    cp $exampleLog "$diagnostics/examples-LastTest.log"
  }

  if (Test-Path $build) {
    $generated = @(ls -r -File -Path $build `
      -Include '*.omnirefl.hpp', '*.omnirefl.hpp.d')
    if (0 -lt $generated.Count) {
      Compress-Archive -Path $generated.FullName `
        -DestinationPath "$diagnostics/generated-reflection.zip" -Force
    }
  }

  if (Test-Path $work) { rm -r -Force $work }
}
