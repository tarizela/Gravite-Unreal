@echo off

rem message colors
set RED=[91m
set GREEN=[92m
set YELOW=[93m
set CYAN=[96m
set NC=[0m

setlocal

set gdrive_syncmode=2
set gdrive_driveletter=G

echo Configuring git lfs...
echo:
echo %YELOW%Please specify the used Google Drive syncing mode. The syncing mode can be configured in the preferences of the Google Drive app.%NC%
echo 1: Stream files
echo 2: Mirror files (default)
set /p gdrive_syncmode="Select [2]: "

if %gdrive_syncmode% == 1 (
    goto Syncmode_Stream
) else if %gdrive_syncmode% == 2 (
    goto Syncmode_Mirror
) else (
    goto Error_InvalidSyncMode
)

:Syncmode_Stream
echo %CYAN%Syncing: Stream files [1].%NC%
echo:
echo %YELOW%Please specify the drive letter of the Google Drive.%NC%
set /p gdrive_driveletter="Select [G]: "
echo %CYAN%Using drive '%gdrive_driveletter%'.%NC%
set lfs_root=%gdrive_driveletter%:/My Drive/Gravite-LFS
goto Begin_Setup

:Syncmode_Mirror
echo %CYAN%Syncing: Mirror files [2].%NC%
echo:
set lfs_root=%USERPROFILE%/My Drive/Gravite-LFS
goto Begin_Setup

:Begin_Setup
if exist "%lfs_root%.lnk" (
    rem resolve the symbolic link (pretty hacky)
    for /f %%i in ('find "\Gravite-LFS" "%lfs_root%.lnk"') do set lfs_root=%%i
)

set lfs_root=%lfs_root:\=/%

echo %CYAN%Using directory '%lfs_root%' as LFS database.%NC%

cd "%~dp0"

set script_dir=%~dp0
set root_dir=%script_dir:\=/%
set lfs_folderstore_root=Development/Tools/lfs-folderstore

where /Q git
if ERRORLEVEL 1 (
    goto Error_GitMissing
)

if not exist "%lfs_folderstore_root%/lfs-folderstore.exe" (
    goto Error_LFSFolderstoreMissing
)

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
echo %RED%Directory '%lfs_root%' does not exist. Make sure that you have access to the Google Drive shared Gravite-LFS directory.%NC%
exit /B 102

:Error_InvalidSyncMode
echo %RED%Invalid Google Drive syncing mode '%gdrive_syncmode%'.%NC%
exit /B 103