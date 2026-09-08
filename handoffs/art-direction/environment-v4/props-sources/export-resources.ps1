$ErrorActionPreference='Stop'
$helperText=Get-Content -Raw -LiteralPath 'C:/Paladin/assets/sprites/environment-v4/props/export-props.ps1'
$helperEnd=$helperText.IndexOf('$propRoot =')
if($helperEnd -lt 0){throw 'Prop export helper definition not found'}
Invoke-Expression ($helperText.Substring(0,$helperEnd))
$resourceRoot=Split-Path -Parent $PSCommandPath
$runtimeRoot='C:/Paladin/assets/sprites/environment-v4/props'
$approved=Import-Csv -LiteralPath 'C:/Paladin/handoffs/art-direction/sunlight-and-shadow/palette-64.csv'
$entries=Get-Content -Raw -LiteralPath (Join-Path $resourceRoot 'resources-generation.json') | ConvertFrom-Json
$settings=@{
 'resource-fish'=@(1,2,12,13,17,19,24,33,34,35,37,38,39,40,41,45,51,53,54,55,58,61,62,64)
 'resource-meat'=@(1,2,4,6,10,17,19,20,21,22,24,33,34,35,46,47,51,52,53,54,55,58)
 'resource-food'=@(1,2,9,10,17,18,19,24,28,33,34,35,45,51,52,53,54,55,58,64)
 'resource-stone'=@(10,12,13,29,30,33,34,35,36,37,38,39,40,41,42,60,61,62)
 'resource-lumber'=@(1,2,6,10,17,18,19,24,29,33,34,35,51,52,53,54,55,58)
 'resource-materials'=@(1,2,6,10,17,18,19,24,29,33,34,35,37,39,51,52,53,54,55,58)
}
$report=foreach($entry in $entries){
 $colors=@($approved | Where-Object {[int]$_.Index -in $settings[$entry.name]} | ForEach-Object {$_.Hex})
 $result=[PropExport]::Export($entry.source,(Join-Path $runtimeRoot ($entry.name+'.png')),24,24,[string[]]$colors)
 "$($entry.name): $result"
}
$allowed=@{}; foreach($color in $approved){$allowed[$color.Hex.ToUpperInvariant()]=$true}
foreach($entry in $entries){
 $img=[System.Drawing.Bitmap]::new((Join-Path $runtimeRoot ($entry.name+'.png')));$off=0;$partial=0;$transparent=0;$colors=@{}
 for($y=0;$y -lt $img.Height;$y++){for($x=0;$x -lt $img.Width;$x++){
  $c=$img.GetPixel($x,$y);if($c.A -eq 0){$transparent++;continue};if($c.A -ne 255){$partial++}
  $hex='#{0:X2}{1:X2}{2:X2}' -f $c.R,$c.G,$c.B;$colors[$hex]=$true;if(-not $allowed.ContainsKey($hex)){$off++}
 }}
 $report += "$($entry.name): independently validated visibleColors=$($colors.Count) offPalette=$off partialAlpha=$partial transparent=$transparent colors=$($colors.Keys -join ',')"
 $img.Dispose();if($off -gt 0 -or $partial -gt 0){throw 'Palette or alpha export failure'}
}
$report | Set-Content -LiteralPath (Join-Path $resourceRoot 'resources-validation.txt')
$report
