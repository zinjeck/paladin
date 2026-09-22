param(
    [Parameter(Mandatory=$true)][string]$Baseline,
    [Parameter(Mandatory=$true)][string]$Current,
    [int]$X = 260,
    [int]$Y = 60,
    [int]$Width = 760,
    [int]$Height = 680
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

function Read-Region([string]$Path) {
    $image = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Path).Path)
    try {
        $rect = New-Object System.Drawing.Rectangle $X,$Y,$Width,$Height
        $crop = $image.Clone($rect,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $all = New-Object System.Drawing.Rectangle 0,0,$crop.Width,$crop.Height
            $data = $crop.LockBits(
                $all,
                [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
                [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
            )
            try {
                $count = [Math]::Abs($data.Stride) * $crop.Height
                $bytes = New-Object byte[] $count
                [Runtime.InteropServices.Marshal]::Copy($data.Scan0,$bytes,0,$count)
                return ,$bytes
            }
            finally {
                $crop.UnlockBits($data)
            }
        }
        finally {
            $crop.Dispose()
        }
    }
    finally {
        $image.Dispose()
    }
}

$a = Read-Region $Baseline
$b = Read-Region $Current
if ($a.Length -ne $b.Length) { throw "Region sizes differ." }

$changed = 0
$samples = 0
for ($i = 0; $i -le $a.Length - 3; $i += 64) {
    $diff =
        [Math]::Abs([int]$a[$i] - [int]$b[$i]) +
        [Math]::Abs([int]$a[$i+1] - [int]$b[$i+1]) +
        [Math]::Abs([int]$a[$i+2] - [int]$b[$i+2])
    if ($diff -gt 45) { $changed++ }
    $samples++
}
$pct = if ($samples) { 100.0 * $changed / $samples } else { 0.0 }
Write-Output ([Math]::Round($pct,2).ToString("0.00",[Globalization.CultureInfo]::InvariantCulture))
