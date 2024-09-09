set(
  UPDATER_EXE
  "${CMAKE_CURRENT_BINARY_DIR}/Updater.exe"
)
file(
  DOWNLOAD
  "https://github.com/fredemmott/autoupdates/releases/download/vicius-v1.7.813%2Bfredemmott.1/Updater-Release.exe"
  ${UPDATER_EXE}
  EXPECTED_HASH SHA256=CCA4D201391411AF330645FE17F5A37EB51FF25560B2AF7442AAF80BAB9C1D0F
)

return(PROPAGATE UPDATER_EXE)