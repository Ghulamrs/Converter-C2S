# The differential suite on Windows, over the same cases tests/run.sh drives.
#
# That box has no bash, no git-bash and a wsl.exe with no distribution, so the
# shell script cannot run there at all - and the converter's own suite was the
# one thing the third toolchain never checked. Only the driver is duplicated:
# every case, every .flags and every .expect is the same file both scripts
# read, and the rules below are the rules there, said in PowerShell.
#
# **If you change one, change the other.** The two are kept in step by hand,
# which is a thing this project otherwise refuses to do; the alternative was a
# generator for two twenty-line loops, or leaving Windows unchecked.
#
#   powershell -ExecutionPolicy Bypass -File tests\run.ps1
#
# C2S, CC1 and SHC override where the binaries are, as in the shell version.
# The default is ..\RStudio\x64\Release, the solution's output directory,
# where all five programs and shc's runtime land together.
#
# Two differences from the shell script, both of them Windows and neither of
# them a rule:
#
#   - the built programs are named .exe, because a file without that suffix is
#     not something Windows will run;
#   - output is compared with CRLF folded to LF, since the two runtimes do not
#     have to agree about line endings and the question here is what was
#     printed, not how the lines were ended.

$ErrorActionPreference = 'Continue'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $here

# Where a Windows build leaves all five programs: the solution builds them
# into RStudio's x64\Release together, which is the whole point of building
# them together. Not this repository's own root, which is where the Unix
# script looks and where nothing lands here.
function Default-Tool($envName, $leaf) {
    $named = [Environment]::GetEnvironmentVariable($envName)
    if ($named) { return $named }
    return (Join-Path (Split-Path -Parent $root) "RStudio\x64\Release\$leaf")
}

$c2s = Default-Tool 'C2S' 'c2s.exe'
$cc1 = Default-Tool 'CC1' 'cc1.exe'
$shc = Default-Tool 'SHC' 'shc.exe'
$out = Join-Path $here 'out'
New-Item -ItemType Directory -Force -Path $out | Out-Null

foreach ($pair in @(@('c2s', $c2s), @('cc1', $cc1), @('shc', $shc))) {
    if (-not (Test-Path $pair[1])) {
        Write-Host ("no {0} at {1} - set {2}=" -f $pair[0], $pair[1], $pair[0].ToUpper())
        exit 2
    }
    Write-Host ("{0} {1}" -f $pair[0], $pair[1])
}
Write-Host ""

$script:pass = 0
$script:fail = 0
function Fails($why) { Write-Host "FAIL: $why"; $script:fail++ }

# The .flags a case carries, as an argument array. Empty when there is none.
function FlagsFor($path) {
    if (-not (Test-Path $path)) { return @() }
    return ((Get-Content $path -Raw) -split '\s+') | Where-Object { $_ -ne '' }
}

# Run a program, capture what it wrote and what it returned. Native stderr and
# stdout both land in the file, which is what the shell script's 2> does.
function Run($exe, $arguments, $capture) {
    $text = & $exe @arguments 2>&1 | Out-String
    Set-Content -LiteralPath $capture -Value $text -NoNewline
    return $LASTEXITCODE
}

function Text($path) {
    if (-not (Test-Path $path)) { return '' }
    $t = Get-Content -LiteralPath $path -Raw
    if ($null -eq $t) { return '' }
    return ($t -replace "`r`n", "`n")
}

function StripTrailingSpace($text) {
    return (($text -split "`n") | ForEach-Object { $_ -replace ' +$', '' }) -join "`n"
}

function FirstLine($text) {
    $lines = $text -split "`n"
    if ($lines.Length -gt 0) { return $lines[0] }
    return ''
}

# ---- Shalimar -> C89, byte for byte -----------------------------------------
foreach ($f in Get-ChildItem (Join-Path $here 'cases\s2c\*.shm')) {
    $n = $f.BaseName
    if ((Run $shc @($f.FullName, '-o', "$out\s_$n.exe") "$out\e") -ne 0) {
        Fails "${n}: shc refused the original"; continue }
    if ((Run $c2s @($f.FullName, '-o', "$out\conv_$n.c") "$out\e") -ne 0) {
        Fails ("{0}: markers or refusal: {1}" -f $n, (FirstLine (Text "$out\e"))); continue }
    if ((Run $cc1 @("$out\conv_$n.c", '-o', "$out\c_$n.exe") "$out\e") -ne 0) {
        Fails ("{0}: cc1 refused the conversion: {1}" -f $n, (FirstLine (Text "$out\e"))); continue }
    Run "$out\s_$n.exe" @() "$out\o1" | Out-Null
    Run "$out\c_$n.exe" @() "$out\o2" | Out-Null
    if ((Text "$out\o1") -ceq (Text "$out\o2")) { $script:pass++ }
    else { Fails "${n}: outputs differ" }
}

# ---- C89 -> Shalimar, identical but for the trailing space ------------------
foreach ($f in Get-ChildItem (Join-Path $here 'cases\c2s\*.c')) {
    $n = $f.BaseName
    if ((Run $cc1 @($f.FullName, '-o', "$out\c_$n.exe") "$out\e") -ne 0) {
        Fails "${n}: cc1 refused the original"; continue }
    if ((Run $c2s @($f.FullName, '-o', "$out\conv_$n.shm") "$out\e") -ne 0) {
        Fails ("{0}: markers or refusal: {1}" -f $n, (FirstLine (Text "$out\e"))); continue }
    if ((Run $shc @("$out\conv_$n.shm", '-o', "$out\s_$n.exe") "$out\e") -ne 0) {
        Fails ("{0}: shc refused the conversion: {1}" -f $n, (FirstLine (Text "$out\e"))); continue }
    Run "$out\c_$n.exe" @() "$out\o1" | Out-Null
    Run "$out\s_$n.exe" @() "$out\o2" | Out-Null
    if ((StripTrailingSpace (Text "$out\o1")) -ceq (StripTrailingSpace (Text "$out\o2"))) { $script:pass++ }
    else { Fails "${n}: outputs differ" }
}

# ---- the one difference: `?` writes a space the format did not have ---------
foreach ($f in Get-ChildItem (Join-Path $here 'cases\spacing\*.c')) {
    $n = $f.BaseName
    if ((Run $cc1 @($f.FullName, '-o', "$out\c_$n.exe") "$out\e") -ne 0) {
        Fails "${n}: cc1 refused the original"; continue }
    if ((Run $c2s @($f.FullName, '-o', "$out\conv_$n.shl") "$out\e") -ne 0) {
        Fails ("{0}: c2s refused it: {1}" -f $n, (FirstLine (Text "$out\e"))); continue }
    if ((Text "$out\e") -notmatch 'warning') {
        Fails "${n}: converted without warning about the spacing"; continue }
    if ((Run $shc @("$out\conv_$n.shl", '-o', "$out\s_$n.exe") "$out\e") -ne 0) {
        Fails ("{0}: shc refused the conversion: {1}" -f $n, (FirstLine (Text "$out\e"))); continue }
    Run "$out\c_$n.exe" @() "$out\o1" | Out-Null
    Run "$out\s_$n.exe" @() "$out\o2" | Out-Null
    if (((Text "$out\o1") -replace ' ', '') -ceq ((Text "$out\o2") -replace ' ', '')) { $script:pass++ }
    else { Fails "${n}: more than the spacing differs" }
}

# ---- the permission cases ---------------------------------------------------
foreach ($f in Get-ChildItem (Join-Path $here 'cases\allow\*.c')) {
    $n = $f.BaseName
    $flags = FlagsFor (Join-Path $here "cases\allow\$n.flags")

    # Refused without the permission first, or the case proves nothing about it.
    if ((Run $c2s @($f.FullName, '-o', "$out\bare_$n.shm") "$out\e") -eq 0) {
        Fails ("{0}: converts without {1}, so it proves nothing about it" -f $n, ($flags -join ' '))
        continue }
    if ((Run $cc1 @($f.FullName, '-o', "$out\c_$n.exe") "$out\e") -ne 0) {
        Fails "${n}: cc1 refused the original"; continue }
    if ((Run $c2s (@($f.FullName, '-o', "$out\conv_$n.shm") + $flags) "$out\e") -ne 0) {
        Fails ("{0}: markers or refusal under {1}: {2}" -f $n, ($flags -join ' '), (FirstLine (Text "$out\e")))
        continue }
    if ((Run $shc @("$out\conv_$n.shm", '-o', "$out\s_$n.exe") "$out\e") -ne 0) {
        Fails ("{0}: shc refused the conversion: {1}" -f $n, (FirstLine (Text "$out\e"))); continue }
    Run "$out\c_$n.exe" @() "$out\o1" | Out-Null
    Run "$out\s_$n.exe" @() "$out\o2" | Out-Null
    if ((StripTrailingSpace (Text "$out\o1")) -ceq (StripTrailingSpace (Text "$out\o2"))) { $script:pass++ }
    else { Fails ("{0}: outputs differ under {1}" -f $n, ($flags -join ' ')) }
}

# ---- refusals, both directions ----------------------------------------------
#
# Exit 1, markers in the file, every refusal counted also shown, and any
# patterns the case names among them. The counted-equals-shown check is the
# one that has caught three separate faults, each of which left a smaller
# program that compiled and ran.
function CheckRefusals($dir, $sourceGlob, $sourceSuffix, $outSuffix, $validate) {
    foreach ($f in Get-ChildItem (Join-Path $here $sourceGlob)) {
        $n = $f.BaseName
        $ok = & $validate $f.FullName $n
        if (-not $ok) { continue }

        $flags = FlagsFor (Join-Path $here "cases\$dir\$n.flags")
        $rc = Run $c2s (@($f.FullName, '-o', "$out\conv_$n$outSuffix") + $flags) "$out\e"
        if ($rc -ne 1) { Fails "${n}: expected exit 1 with markers, got $rc"; continue }

        $written = Text "$out\conv_$n$outSuffix"
        $printed = ([regex]::Matches($written, 'BEYOND SHALIMAR')).Count
        if ($printed -eq 0) { Fails "${n}: no markers in the output"; continue }

        $said = Text "$out\e"
        $m = [regex]::Match($said, ': (\d+) constructs? ha[sv]e? no expression')
        if (-not $m.Success) { Fails "${n}: the run did not say how many it refused"; continue }
        $counted = [int]$m.Groups[1].Value
        if ($printed -ne $counted) {
            Fails "${n}: $counted refused, $printed marked - a refusal went missing"; continue }

        $expect = Join-Path $here "cases\$dir\$n.expect"
        if (Test-Path $expect) {
            $missing = 0
            foreach ($want in (Get-Content $expect)) {
                if ($want -eq '') { continue }
                if ($written -notmatch [regex]::Escape($want)) {
                    Write-Host "    ${n}: nothing refused for: $want"
                    $missing++
                }
            }
            if ($missing -ne 0) { Fails "${n}: $missing expected refusal(s) missing"; continue }
        }
        $script:pass++
    }
}

CheckRefusals 'beyond' 'cases\beyond\*.c' '.c' '.shm' {
    param($path, $n)
    # Valid C89 first, or the case proves the parser choked rather than that
    # the mapping refused. -c because a case need not link.
    if ((Run $cc1 @('-c', $path, '-o', "$out\b_$n.obj") "$out\e") -ne 0) {
        Fails ("{0}: cc1 refused the case itself: {1}" -f $n, (FirstLine (Text "$out\e")))
        return $false
    }
    return $true
}

CheckRefusals 's2cbeyond' 'cases\s2cbeyond\*.shm' '.shm' '.c' {
    param($path, $n)
    if ((Run $shc @($path, '-o', "$out\sb_$n.exe") "$out\e") -ne 0) {
        Fails ("{0}: shc refused the case itself: {1}" -f $n, (FirstLine (Text "$out\e")))
        return $false
    }
    return $true
}

# ---- the preprocessor decision list -----------------------------------------
foreach ($f in Get-ChildItem (Join-Path $here 'cases\defines\*.c')) {
    $n = $f.BaseName
    $rc = Run $c2s @($f.FullName) "$out\e"
    if ($rc -ne 1) { Fails "${n}: expected exit 1 with the decision list, got $rc"; continue }
    if ((Text "$out\e") -notmatch 'preprocessor construct') { Fails "${n}: no decision list"; continue }
    $script:pass++
}

# ---- the canonical identities ----------------------------------------------
foreach ($f in Get-ChildItem (Join-Path $here 'cases\s2c\*.shm')) {
    $n = $f.BaseName
    if (Test-Path (Join-Path $here "cases\s2c\$n.nocanon")) { continue }
    if ((Run $c2s @('--canon', $f.FullName, '-o', "$out\canon_$n.shm") "$out\e") -ne 0) {
        Fails "canon ${n}: refused"; continue }
    if ((Run $shc @("$out\canon_$n.shm", '-o', "$out\cs_$n.exe") "$out\e") -ne 0) {
        Fails "canon ${n}: shc refused"; continue }
    Run "$out\s_$n.exe" @() "$out\o1" | Out-Null
    Run "$out\cs_$n.exe" @() "$out\o2" | Out-Null
    if ((Text "$out\o1") -ceq (Text "$out\o2")) { $script:pass++ }
    else { Fails "canon ${n}: outputs differ" }
}

Write-Host ""
Write-Host ("pass={0} fail={1}" -f $script:pass, $script:fail)
if ($script:fail -ne 0) { exit 1 }
exit 0
