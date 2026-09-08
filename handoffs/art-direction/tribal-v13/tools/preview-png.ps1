param([string]$Project='C:/Paladin')
Add-Type -AssemblyName System.Drawing
Get-ChildItem "$Project/handoffs/art-direction/tribal-v13/previews/*.bmp" | ForEach-Object {
    $image=[Drawing.Bitmap]::new($_.FullName)
    try {$image.Save([IO.Path]::ChangeExtension($_.FullName,'png'),[Drawing.Imaging.ImageFormat]::Png)}
    finally {$image.Dispose()}
}
