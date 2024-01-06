$nativeSize = 32
$nativeDpi = 96
$sizes = 16,32,48,64,128,256
$intermediates = @()
foreach ($size in $sizes) {
  $out = "icon-${size}.png"
  magick convert `
    -background none `
    -density $(($nativeDpi * $size) / $nativeSize) `
    "$(Get-Location)\icon.svg" `
    -gravity center `
    -extent "${size}x${size}" `
    png:$out
  $intermediates += $out
}
Remove-Item icon.ico
magick convert `
  -background none `
  @intermediates `
  icon.ico
Remove-Item $intermediates
