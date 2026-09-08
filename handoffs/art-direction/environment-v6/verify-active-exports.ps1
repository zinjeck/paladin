$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$project = 'C:/Paladin'
$approved=@{}
Import-Csv "$project/handoffs/art-direction/sunlight-and-shadow/palette-64.csv" | ForEach-Object {$approved[$_.Hex.TrimStart('#')]=1}
$files=Get-Content "$project/assets/sprites/sprites.catalog" | Where-Object {$_ -and -not $_.StartsWith('#')} | ForEach-Object {($_ -split '\s+')[1]} | Sort-Object -Unique
$report=foreach($path in $files){
    $b=[System.Drawing.Bitmap]::new("$project/assets/sprites/$path")
    $bad=0;$partial=0;$colors=@{};$visible=0
    for($y=0;$y -lt $b.Height;$y++){for($x=0;$x -lt $b.Width;$x++){
        $c=$b.GetPixel($x,$y);if($c.A -eq 0){continue};$visible++
        $h='{0:X2}{1:X2}{2:X2}' -f $c.R,$c.G,$c.B;$colors[$h]=1
        if(-not $approved.ContainsKey($h)){$bad++}
        if($c.A -ne 255){$partial++}
    }}
    [pscustomobject]@{File=$path;Width=$b.Width;Height=$b.Height;Colors=$colors.Count;VisiblePixels=$visible;OffPalette=$bad;PartialAlpha=$partial}
    $b.Dispose()
    if($bad -or $partial -or -not $visible){throw "Invalid export $path"}
}
$report | Export-Csv "$project/handoffs/art-direction/environment-v6/active-sprite-verification.csv" -NoTypeInformation
"Verified $($report.Count) unique active PNGs; zero off-palette or partial-alpha pixels."
