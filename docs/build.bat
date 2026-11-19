@echo off
REM ============================================================
REM  RAN Online EP7 - Auto Build Script (Windows + VS2008)
REM  วางไฟล์นี้ไว้ในโฟลเดอร์เดียวกับ Smtm.sln แล้วรัน
REM ============================================================
setlocal enabledelayedexpansion

echo ============================================
echo  RAN EP7 Build Script (VS2008 / Win32)
echo ============================================
echo.

REM ---- 1) หา Visual Studio 2008 ----
set "VS_IDE="
if exist "%VS90COMNTOOLS%..\IDE\devenv.com" set "VS_IDE=%VS90COMNTOOLS%..\IDE\devenv.com"
if exist "C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE\devenv.com" set "VS_IDE=C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE\devenv.com"
if exist "C:\Program Files\Microsoft Visual Studio 9.0\Common7\IDE\devenv.com" set "VS_IDE=C:\Program Files\Microsoft Visual Studio 9.0\Common7\IDE\devenv.com"

if "%VS_IDE%"=="" (
    echo [ERROR] ไม่พบ Visual Studio 2008 (devenv.com)
    echo         กรุณาติดตั้ง VS2008 Professional SP1 ก่อน
    echo         ^(Express Edition ใช้ไม่ได้ เพราะไม่มี MFC^)
    pause
    exit /b 1
)
echo [OK] พบ VS2008: %VS_IDE%
echo.

REM ---- 2) ลบไฟล์ SourceSafe ที่ค้างอยู่ ----
echo [2/5] ลบไฟล์ Visual SourceSafe bindings...
del /s /q "%~dp0vssver2.scc" >nul 2>&1
del /s /q "%~dp0*.vspscc" >nul 2>&1
del /s /q "%~dp0*.vssscc" >nul 2>&1
echo      เสร็จ
echo.

REM ---- 3) สร้าง sql\include ที่หายไป (ODBC headers) ----
echo [3/5] แก้ปัญหาโฟลเดอร์ sql\include ที่ขาดหาย...
if not exist "%~dp0sql\include" mkdir "%~dp0sql\include"

set "SDK_INC="
if exist "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Include\sqlext.h" set "SDK_INC=C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Include"
if exist "C:\Program Files (x86)\Microsoft SDKs\Windows\v6.0A\Include\sqlext.h" set "SDK_INC=C:\Program Files (x86)\Microsoft SDKs\Windows\v6.0A\Include"
if exist "C:\Program Files\Microsoft SDKs\Windows\v6.0A\Include\sqlext.h" set "SDK_INC=C:\Program Files\Microsoft SDKs\Windows\v6.0A\Include"

if not "%SDK_INC%"=="" (
    for %%H in (sql.h sqlext.h sqltypes.h sqlucode.h odbcinst.h) do (
        if exist "%SDK_INC%\%%~nxh" copy /y "%SDK_INC%\%%~nxh" "%~dp0sql\include\" >nul 2>&1
    )
    echo      คัดลอก ODBC headers จาก %SDK_INC% เสร็จ
) else (
    echo      [WARN] ไม่พบ Windows SDK - ถ้า build เจอ error เรื่อง sql.h/sqlext.h
    echo             ให้ติดตั้ง Windows SDK แล้วคัดลอก sql*.h ไปที่ sql\include
)
echo.

REM ---- 4) เลือก Configuration ----
echo [4/5] เลือก Configuration ที่จะ build:
echo        1 = Release_Thailand^|Win32  (เซิร์ฟเวอร์ไทย)
echo        2 = Release^|Win32           (ทั่วไป)
echo        3 = Release_Global^|Win32
echo        4 = Debug^|Win32
set /p CHOICE="เลือก [1-4] (default=1): "
if "%CHOICE%"=="" set CHOICE=1
if "%CHOICE%"=="1" set "CFG=Release_Thailand|Win32"
if "%CHOICE%"=="2" set "CFG=Release|Win32"
if "%CHOICE%"=="3" set "CFG=Release_Global|Win32"
if "%CHOICE%"=="4" set "CFG=Debug|Win32"
echo      จะ build: %CFG%
echo.

REM ---- 5) Build ----
echo [5/5] เริ่ม build... (อาจใช้เวลา 10-30 นาที)
echo.
"%VS_IDE%" "%~dp0Smtm.sln" /build "%CFG%" /out "%~dp0build_log.txt"

echo.
echo ============================================
if errorlevel 1 (
    echo  BUILD ล้มเหลว - ดูรายละเอียดใน build.log / build_log.txt
    echo  ส่งไฟล์ log มาให้ผมช่วยดู error ได้ครับ
) else (
    echo  BUILD สำเร็จ!
    echo  ไฟล์ .exe อยู่ในโฟลเดอร์ของแต่ละโปรเจกต์
    echo  เช่น GameServer\%CFG:|=^%\GameServer.exe
)
echo ============================================
pause
