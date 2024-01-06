# If you're using this as an example for something else and having trouble:
# imagemagick requires that '(' and ')' are standalone arguments (i.e. distinct
# entries in argv) - "(foo)" is invalid, but "(" "foo" ")" is valid.
#
# Here, I use `--%` to disable powershell's usual argument parsing for the rest
# of the command
magick convert `
  -background none `
  -density 256 `
  "$(Get-Location)\icon.svg" `
  -gravity center `
  -define "icon:auto-resize" icon.ico