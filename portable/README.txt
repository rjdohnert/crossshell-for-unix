CrossShell for UNIX - Portable Windows ZIP

PORTABLE USE (no installation or administrator rights required)
Extract the entire ZIP to a folder of your choice. Run the utilities directly
from the extracted bin folder. Nothing automatically copies files or changes PATH.
Optionally add the extracted bin folder to your user PATH.

OPTIONAL SYSTEM-WIDE INSTALLATION
Open Windows PowerShell or PowerShell as administrator, change to the extracted
folder, and run:

  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Install.ps1

This copies the executables to C:\Program Files\bin and appends that directory
to the system PATH if not already present. Existing files with the same names
in that directory are overwritten. Other files are not removed. Existing PATH
entries are preserved. Commands earlier in PATH take precedence.

To preview without changing files or PATH:

  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Install.ps1 -WhatIf

Sign out and back in after installation so new terminals inherit the updated
PATH. This optional script is not an MSI installation and does not create an
entry in Installed apps. To undo it, remove C:\Program Files\bin from the system
PATH and remove only the files installed by this package; keep unrelated files.

The ExecutionPolicy option applies only to this PowerShell process. It does not
change your saved execution policy or override organizational policy.
