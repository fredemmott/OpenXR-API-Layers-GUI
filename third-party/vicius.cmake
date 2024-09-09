set(
  UPDATER_EXE
  "${CMAKE_CURRENT_BINARY_DIR}/Updater.exe"
)
file(
  DOWNLOAD
  "https://github.com/fredemmott/autoupdates/releases/download/vicius-v1.7.813%2Bfredemmott.2/Updater-Release.exe"
  ${UPDATER_EXE}
  EXPECTED_HASH SHA256=386A226F0BE1754607F3CEAD4C242566B22D6532E6A9253A23DC666C6589265C
)

return(PROPAGATE UPDATER_EXE)