param([string]$Project='C:/Paladin')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
foreach($name in @('tribal-day','tribal-night')) {
    $image=[Drawing.Bitmap]::new("$Project/handoffs/art-direction/tribal-v11/previews/$name.bmp")
    try {$image.Save("$Project/handoffs/art-direction/tribal-v11/previews/$name.png",[Drawing.Imaging.ImageFormat]::Png)}
    finally {$image.Dispose()}
}
