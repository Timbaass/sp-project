$ErrorActionPreference = "Stop"

$exe = Join-Path $PSScriptRoot "minifs.exe"
$disk = Join-Path $PSScriptRoot "test_disk.bin"

function Remove-TestArtifacts {
    if (Test-Path $exe) {
        Remove-Item $exe
    }
    if (Test-Path $disk) {
        Remove-Item $disk
    }
}

function Build-Project {
    $gcc = Get-Command gcc -ErrorAction SilentlyContinue
    $cl = Get-Command cl -ErrorAction SilentlyContinue

    if ($gcc) {
        & gcc -std=c11 -Wall -Wextra -O2 main.c commands.c mfs.c -o minifs.exe
        return
    }

    if ($cl) {
        & cl /std:c11 /W4 main.c commands.c mfs.c /Fe:minifs.exe
        return
    }

    throw "C derleyicisi bulunamadi. GCC/MinGW kurun veya Visual Studio Developer PowerShell icinde calistirin."
}

function Run-Step($description, $command) {
    Write-Host "`n> $description"
    & $exe @command
}

Remove-TestArtifacts
Build-Project

Run-Step "Disk olustur" @("init", $disk, "8")
Run-Step "Dosya olustur" @("create", $disk, "not.txt")
Run-Step "Dosyaya yaz" @("write", $disk, "not.txt", "Merhaba", "mini", "dosya", "sistemi")

Write-Host "`n> Dosyayi oku"
$readOutput = & $exe read $disk not.txt
Write-Host $readOutput
if ($readOutput -ne "Merhaba mini dosya sistemi") {
    throw "Okunan veri beklenen veriyle ayni degil."
}

Run-Step "Dosyalari listele" @("list", $disk)
Run-Step "Dosya sistemi durumunu goster" @("status", $disk)
Run-Step "Dosyayi sil" @("delete", $disk, "not.txt")
Run-Step "Silindikten sonra listele" @("list", $disk)

Write-Host "`nTum temel testler basarili."
