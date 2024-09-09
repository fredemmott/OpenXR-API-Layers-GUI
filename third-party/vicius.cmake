set(
  UPDATER_EXE
  "${CMAKE_CURRENT_BINARY_DIR}/Updater.exe"
)
file(
  DOWNLOAD
  "https://github.com/fredemmott/autoupdates/releases/download/vicius-v1.7.813%2Bfredemmott.3/Updater-Release.exe"
  ${UPDATER_EXE}
  EXPECTED_HASH SHA256=3826B6461A9B865DB9307AECED11F33B07A21A4F9A849DBAA07C9C1DE9F012AC
)

return(PROPAGATE UPDATER_EXE)