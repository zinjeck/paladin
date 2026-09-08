param([string]$Project='C:/Paladin')
$ErrorActionPreference='Stop'
$helper=Get-Content "$Project/handoffs/art-direction/tribal-v10/tools/export.ps1" -Raw
Invoke-Expression ($helper.Substring($helper.IndexOf('Add-Type -AssemblyName'),$helper.IndexOf('$palette=@')-$helper.IndexOf('Add-Type -AssemblyName')))
$palette=@(Get-Content "$Project/config/art-palette.hex" | Where-Object {$_ -match '^#[0-9A-Fa-f]{6}$'})
[TribalExport]::Export("$Project/handoffs/art-direction/world-cartography/source/hill.png","$Project/assets/sprites/world-relief/hill.png",0,0,1536,1024,48,32,$palette,$false)
[TribalExport]::Remap("$Project/assets/sprites/world-relief/hill.png","$Project/assets/sprites/world-relief/hill-final.png",@('#A6CD59','#79B56D'),@('#79B56D','#49975B'))
Move-Item -Force "$Project/assets/sprites/world-relief/hill-final.png" "$Project/assets/sprites/world-relief/hill.png"
[TribalExport]::Remap("$Project/assets/sprites/world-relief/hill.png","$Project/assets/sprites/world-relief/hill-earth.png",@('#A99478','#596679'),@('#B78350','#886044'))
Move-Item -Force "$Project/assets/sprites/world-relief/hill-earth.png" "$Project/assets/sprites/world-relief/hill.png"
