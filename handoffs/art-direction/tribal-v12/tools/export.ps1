param([string]$Project='C:/Paladin')
$ErrorActionPreference='Stop'
& "$Project/handoffs/art-direction/tribal-v11/tools/export.ps1" -Project $Project
$dest="$Project/assets/sprites/tribal-v12"
$earth=@('#BD864C','#D5A454','#886044','#7A5038','#633E4B','#49352F','#A99478','#D9C79F')
# Restore actual straw detail from the original unified roof, not the flat trial.
[TribalExport]::Export("$Project/handoffs/art-direction/tribal-v10/source/tribal-atlas.png","$dest/roof.png",52,139,528,363,96,88,$earth,$true)
[TribalExport]::Rotate("$dest/roof.png","$dest/roof-side.png")
# Use the original grass reference without averaging away its clusters.
# Omit the pale sage midtone from the base field; retain rich jade/emerald.
$greens=@('#235747','#337A58','#49975B','#A6CD59','#D0E58A')
$grass="$Project/handoffs/art-direction/tribal-v10/source/grass.png"
$g=[Drawing.Bitmap]::new($grass);$w=$g.Width;$h=$g.Height;$g.Dispose()
[TribalExport]::Export($grass,"$dest/grass.png",0,0,$w,$h,32,32,$greens,$false)
[TribalExport]::Remap("$dest/grass.png","$dest/grass-cold.png",@('#D0E58A','#A6CD59'),@('#A6CD59','#79B56D'))
[TribalExport]::Remap("$dest/grass.png","$dest/grass-warm.png",@('#D0E58A'),@('#A6CD59'))
[TribalExport]::Remap("$dest/grass.png","$dest/grass-jungle.png",@('#D0E58A','#A6CD59','#49975B'),@('#A6CD59','#49975B','#337A58'))
for($i=1;$i -le 4;$i++) {
    $x=(($i-1)%2)*[int]($w/2);$y=[int][Math]::Floor(($i-1)/2)*[int]($h/2)
    [TribalExport]::Export($grass,"$dest/grass-$i.png",$x,$y,[int]($w/2),[int]($h/2),32,32,$greens,$false)
    [TribalExport]::Remap("$dest/grass-$i.png","$dest/grass-cold-$i.png",@('#D0E58A','#A6CD59'),@('#A6CD59','#79B56D'))
    [TribalExport]::Remap("$dest/grass-$i.png","$dest/grass-warm-$i.png",@('#D0E58A'),@('#A6CD59'))
    [TribalExport]::Remap("$dest/grass-$i.png","$dest/grass-jungle-$i.png",@('#D0E58A','#A6CD59','#49975B'),@('#A6CD59','#49975B','#337A58'))
}
$tufts="$Project/handoffs/art-direction/tribal-v12/source/grass-tufts.png"
$g=[Drawing.Bitmap]::new($tufts);$w=$g.Width;$h=$g.Height
if($g.GetPixel(0,0).A -ne 0){$g.Dispose();throw 'Tuft source must have real transparency'}
$g.Dispose()
for($i=1;$i -le 4;$i++) {
    $x=(($i-1)%2)*[int]($w/2);$y=[int][Math]::Floor(($i-1)/2)*[int]($h/2)
    [TribalExport]::Export($tufts,"$dest/tuft-$i.png",$x,$y,[int]($w/2),[int]($h/2),16,16,$greens,$false)
}
