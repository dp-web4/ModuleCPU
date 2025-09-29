@echo off
echo Building EEPROM Editor...
dotnet build -c Release
if %errorlevel% neq 0 (
    echo Build failed!
    pause
    exit /b %errorlevel%
)
echo.
echo Build successful!
echo Output: bin\Release\net6.0-windows\EEPROMEditor.exe
pause