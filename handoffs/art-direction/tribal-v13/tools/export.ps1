param([string]$Project='C:/Paladin')
$ErrorActionPreference='Stop'
# Reuse the established exact-palette export utility without regenerating older art.
$helper=Get-Content "$Project/handoffs/art-direction/tribal-v10/tools/export.ps1" -Raw
Invoke-Expression ($helper.Substring($helper.IndexOf('Add-Type -AssemblyName'),$helper.IndexOf('$palette=@')-$helper.IndexOf('Add-Type -AssemblyName')))
$dest="$Project/assets/sprites/tribal-v13"
$palette=@(Get-Content "$Project/config/art-palette.hex" | Where-Object {$_ -match '^#[0-9A-Fa-f]{6}$'})
$source="$Project/handoffs/art-direction/tribal-v13/source/details-atlas.png"
$names=@('awning','jars','woodrack','basket','hide','shutters','bed','mountain-a','mountain-b')
for($i=0;$i -lt 9;$i++) {
    $x=($i%3)*418;$y=[int][Math]::Floor($i/3)*418
    if($i -ge 6){$y=772}
    $h=if($i -ge 6){441}else{402}
    $tw=if($i -ge 7){64}elseif($i -eq 6){24}else{24}
    $th=if($i -ge 7){64}elseif($i -eq 6){32}else{24}
    $colors=if($i -ge 7){@('#35383E','#394658','#596679','#716D70','#392B3C','#202C43','#9AA7AF')}else{$palette}
    [TribalExport]::Export($source,"$dest/$($names[$i]).png",$x,$y,418,$h,$tw,$th,$colors,$false)
}
$cloth=@(@('#A63545','#D75056'),@('#3F5F9A','#548AC4'),@('#6F4A7E','#8A67B4'),@('#337A58','#49975B'),@('#B78350','#BD864C'),@('#D9C79F','#EFE2CF'),@('#235747','#63BFC3'),@('#874D50','#CB7C70'))
for($i=0;$i -lt $cloth.Count;$i++) {
    [TribalExport]::Remap("$dest/bed.png","$dest/bed-$i.png",@('#A63545','#D75056'),$cloth[$i])
}
Get-ChildItem "$Project/assets/sprites/tribal-v12/grass*.png" | ForEach-Object {
    [TribalExport]::Remap($_.FullName,"$dest/$($_.Name)",@('#235747','#A6CD59','#D0E58A'),@('#337A58','#79B56D','#A6CD59'))
}
# Road material is separate from the house floor even though they shared the old source.
$road="$Project/handoffs/art-direction/tribal-v13/source/road.png"
$b=[Drawing.Bitmap]::new($road);$w=$b.Width;$h=$b.Height;$b.Dispose()
[TribalExport]::Export($road,"$dest/road.png",0,0,[int]($w/3),[int]($h/3),32,32,@('#4E3B39','#74513F','#7A5038','#874D50','#95655F','#A99478'),$false)
foreach($kind in @('deep','shallow')) {
    $water="$Project/assets/sprites/environment-v7/water-$kind.png"
    $b=[Drawing.Bitmap]::new($water);$w=$b.Width;$h=$b.Height;$b.Dispose()
    $colors=if($kind -eq 'deep'){@('#202C43','#3F5F9A','#548AC4')}else{@('#3F5F9A','#548AC4','#63BFC3')}
    [TribalExport]::Export($water,"$dest/water-$kind.png",0,0,$w,$h,$w,$h,$colors,$false)
}

[TribalExport]::Remap("$Project/assets/sprites/environment-v4/nature/mountain.png","$dest/mountain-rock-dark.png",@('#9AA7AF','#716D70','#A99478','#D7E0E3'),@('#394658','#202C43','#35383E','#596679'))
