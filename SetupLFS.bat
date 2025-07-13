@echo off

rem message colors
set RED=[91m
set GREEN=[92m
set YELOW=[93m
set CYAN=[96m
set NC=[0m

echo Configuring git lfs...

setlocal
cd "%~dp0"

set script_dir=%~dp0
set root_dir=%script_dir:\=/%
set lfs_root=%USERPROFILE:\=/%/My Drive/Gravite-LFS
set lfs_folderstore_root=Development/Tools/lfs-folderstore

where /Q git
if ERRORLEVEL 1 (
    goto Error_GitMissing
)

if not exist "%lfs_folderstore_root%/lfs-folderstore.exe" (
    goto Error_LFSFolderstoreMissing
)

echo %CYAN%Using directory '%lfs_root%' as LFS database.%NC%

if not exist "%lfs_root%/" (
    goto Error_LFSDatabaseMissing
)

rem setup LFS
git config lfs.standalonetransferagent lfs-folderstore
git config lfs.customtransfer.lfs-folderstore.path "%root_dir%Development/Tools/lfs-folderstore/lfs-folderstore.exe"
git config lfs.customtransfer.lfs-folderstore.args "'%lfs_root%'"

echo %GREEN%Setup complete.%NC%

exit /B 0

:Error_GitMissing
echo %RED%Cannot access git. Make sure that git is installed and is part of the PATH environment variable.%NC%
exit /B 100

:Error_LFSFolderstoreMissing
echo %RED%lfs-folderstore executable is missing. Make sure that the executable is in '%root_dir%%lfs_folderstore_root%'.%NC%
exit /B 101

:Error_LFSDatabaseMissing
echo %RED%Directory '%lfs_root%' does not exist. Make sure that you have access to the Google Drive shared Gravite-LFS directory and that the Google Drive app runs with the 'Mirror files' syncing mode.%NC%
exit /B 102