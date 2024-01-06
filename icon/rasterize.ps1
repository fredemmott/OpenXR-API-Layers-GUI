magick convert `
  -background none `
  -density 256 `
  "$(Get-Location)\icon.svg" `
  -gravity center `
  -define "icon:auto-resize" icon.ico