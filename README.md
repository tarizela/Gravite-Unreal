# Gravity

## Building

### Prerequisites

#### Unreal Engine

This project requires a custom build of the Unreal engine. You can find the engine code here [github.com/tarizela/UnrealEngine](https://github.com/tarizela/UnrealEngine).
Setup the engine as described in the official Unreal engine documentation and build it by running the Setup.bat and GenerateProjectFiles.bat followed by BuildInstalled.bat. The BuildInstalled.bat creates a standalone build of the engine, similar to the one that you download with the Epic launcher.

#### Google Drive

The Unreal assets and other binary files are stored on Google Drive (GDrive). We use it as a replacement for the LFS server. You will need the GDrive app and access to the Gravite-LFS directory for syncing of the LFS files. The app can be downloaded from here [dl.google.com/drive-file-stream/GoogleDriveSetup.exe](https://dl.google.com/drive-file-stream/GoogleDriveSetup.exe).

After you install the app, you will have to create a GDrive shortcut to the LFS directory.

1. Navigate to "Shared with me" section and find the Gravite-LFS directory.
2. Right click on it and select "Organize -> Add shortcut". This will open a new window asking you where to place the new shortcut. Select "My Drive" and click on "Add".
3. GDrive will start to syncing the Gravite-LFS directory to your system. Wait until the download is complete.

### Cloning the Repo

Clone the repo with a git client of your choice and then run the SetupLFS.bat. The script will configure LFS for GDrive.

## Pushing Changes

To get your changes on the main branch, you have to create a new branch, push your changes and then create a new pull request (PR) on github. After a review the PR will be approved and merged into main. You can configure the PR to automatically delete the branch when the merge completes. It's better to create new branches for different changes then keep working on the same branch indefinitely.

### Things to Store in the Repo

The repo should only contain source code, and Unreal assets. Do not push source assets because this will take too much LFS (GDrive) space.

Things that should be pushed:
* Source code
* Unreal assets (.uasset)
* ThirdParty dependencies

Things **not** to store:
* Source assets (.fbx, .png, .mp3, etc.)