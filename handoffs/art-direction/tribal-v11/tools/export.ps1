param([string]$Project='C:/Paladin')
$ErrorActionPreference='Stop'
# Reproduce the unchanged base set and reuse its exact-palette export helper.
& "$Project/handoffs/art-direction/tribal-v10/tools/export.ps1" -Project $Project
$earth=@('#BD864C','#D5A454','#886044','#7A5038','#633E4B','#49352F','#A99478','#D9C79F','#392B3C')
$source="$Project/handoffs/art-direction/tribal-v11/source/house-facades.png"
$dest="$Project/assets/sprites/tribal-v11"
[TribalExport]::Export($source,"$dest/house-front.png",87,89,1485,364,96,28,$earth,$true)
[TribalExport]::Export($source,"$dest/house-back.png",87,516,1485,359,96,28,$earth,$true)
