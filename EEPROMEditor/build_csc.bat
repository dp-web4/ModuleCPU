@echo off
echo Building EEPROM Editor with CSC...
echo.

REM Try to find csc.exe in common locations
set CSC_PATH=
if exist "C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe" (
    set CSC_PATH=C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe
) else if exist "C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.exe" (
    set CSC_PATH=C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.exe
) else (
    echo ERROR: Could not find csc.exe
    echo Please ensure .NET Framework is installed
    pause
    exit /b 1
)

echo Found CSC at: %CSC_PATH%
echo.

REM Compile the application
"%CSC_PATH%" /target:winexe /out:EEPROMEditor.exe /reference:System.dll /reference:System.Drawing.dll /reference:System.Windows.Forms.dll EEPROMEditor_Framework.cs

if %errorlevel% neq 0 (
    echo.
    echo Build failed!
    pause
    exit /b %errorlevel%
)

echo.
echo Build successful!
echo Output: EEPROMEditor.exe
echo.
echo You can now run EEPROMEditor.exe
pause