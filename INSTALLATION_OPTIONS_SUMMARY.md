# ?? OPENCV INSTALLATION - MULTIPLE OPTIONS AVAILABLE

**Status:** ?? **Admin required for automated script**  
**Alternative:** Manual or pre-configured options available

---

## ?? YOUR OPTIONS

### **Option 1: Run with Admin Privileges** ? (FASTEST)

```powershell
# Right-click PowerShell ? Run as Administrator ? then run:
cd "C:\Users\Admin\source\repos\bullet_hole_detection_system"
.\install_opencv.ps1
```

**Time:** ~40 minutes  
**Result:** Full automated installation  
**Requires:** Administrator access

---

### **Option 2: Use Simplified Batch Script** ? (NO ADMIN)

```cmd
REM No admin needed, run directly:
cd "C:\Users\Admin\source\repos\bullet_hole_detection_system"
build_with_opencv.bat
```

**What it does:**
- ? Checks for vcpkg
- ? Checks for CMake
- ? Checks for VS2022
- ? Configures CMake
- ? Builds project
- ? Creates executable

**Requires:** OpenCV already installed (via vcpkg or pre-built)

---

### **Option 3: Manual Steps** (MOST CONTROL)

Follow: `docs/OPENCV_INSTALLATION_GUIDE.md`

**Steps:**
1. Download & install vcpkg manually
2. Run: `vcpkg install opencv:x64-windows`
3. Run CMake manually
4. Build in Visual Studio

---

## ?? RECOMMENDED PATH

### **IF you have Admin access:**
```powershell
# Open PowerShell as Administrator
.\install_opencv.ps1
```

### **IF you DON'T have Admin:**
```
1. Ask IT to install vcpkg + OpenCV (takes 1 hour)
   OR
2. Get pre-built OpenCV binaries from https://opencv.org/releases/
3. Extract to C:\opencv (or your local folder)
4. Run: build_with_opencv.bat
```

---

## ?? INSTALLATION METHODS COMPARISON

| Method | Admin? | Time | Ease | Automation |
|--------|--------|------|------|-----------|
| **PowerShell script** | ? Required | 40 min | Easy | Full |
| **Batch script** | ? No | 10 min* | Medium | Partial** |
| **Manual vcpkg** | ? Required | 45 min | Medium | None |
| **Pre-built binaries** | ? No | 30 min* | Hard | None |

*If OpenCV already installed  
**Requires OpenCV already present

---

## ?? QUICK DECISION TREE

```
Do you have Admin access?
?
?? YES ? Use: .\install_opencv.ps1
?        Time: ~40 min
?        Result: Everything automated
?
?? NO  ? Two options:
         ?
         ?? Ask IT to install vcpkg + OpenCV
         ?  Then: Use build_with_opencv.bat
         ?
         ?? Download pre-built OpenCV
            Extract to C:\opencv
            Then: Use build_with_opencv.bat
```

---

## ??? WHAT IF I DON'T HAVE OPENCV YET?

### **Path 1: Pre-built Binaries (Easiest)**

```
1. Download from: https://opencv.org/releases/
   - Get: opencv-windows-xxx.exe (4.5+)

2. Run installer
   - Select Visual Studio 2022 (if prompted)
   - Install to: C:\opencv

3. Set environment variable:
   SET OpenCV_DIR=C:\opencv\build

4. Run: build_with_opencv.bat
```

**Time:** ~30 minutes  
**No admin needed**

### **Path 2: Ask IT for vcpkg**

```
Request to IT:
"Please install vcpkg and OpenCV x64:
1. Download https://github.com/Microsoft/vcpkg
2. Extract to C:\vcpkg
3. Run bootstrap: C:\vcpkg\bootstrap-vcpkg.bat
4. Install: C:\vcpkg\vcpkg install opencv:x64-windows"

After they install:
1. Run: build_with_opencv.bat
2. Build succeeds!
```

**Time:** IT setup + 10 min build  
**No admin access needed from you**

---

## ?? FILES PROVIDED FOR YOU

### **Automated Installation**
1. `install_opencv.ps1` - Full automated (needs admin)
2. `build_with_opencv.bat` - Build helper (no admin)

### **Guides**
3. `docs/OPENCV_INSTALLATION_GUIDE.md` - Complete reference
4. `OPENCV_INSTALLATION_READY_TO_START.md` - Quick start

---

## ? AFTER OPENCV IS INSTALLED

```
1. Verify vcpkg has OpenCV:
   ls C:\vcpkg\installed\x64-windows\include\opencv2

2. Verify CMake finds it:
   build_with_opencv.bat

3. Build succeeds:
   cmake --build build --config Release

4. Executable created:
   build\Release\BulletHoleDetection.exe

5. STEP 2 is now complete! ?
```

---

## ?? WHAT TO DO NOW

### **Choose based on your situation:**

**Scenario 1: I have Admin**
```powershell
# Open PowerShell as Administrator
cd "C:\Users\Admin\source\repos\bullet_hole_detection_system"
.\install_opencv.ps1
# Wait 40 minutes
# Done!
```

**Scenario 2: I don't have Admin**
```
1. Get pre-built OpenCV from https://opencv.org/releases/
2. Extract to C:\opencv
3. Run: build_with_opencv.bat
# Done!
```

**Scenario 3: I want manual control**
```
Follow: docs/OPENCV_INSTALLATION_GUIDE.md
Execute steps manually
```

**Scenario 4: I want to skip for now**
```
STEP 3 (Feature extraction) doesn't need compilation yet
Continue with STEP 3 code-only work
```

---

## ?? NEXT STEPS

### **If you can install OpenCV:**
1. Choose your method (automated, batch, or manual)
2. Install OpenCV
3. Run `build_with_opencv.bat`
4. Proceed to STEP 3

### **If you can't install OpenCV:**
1. Ask IT department for help
2. Use pre-built binaries
3. Or skip to STEP 3 (feature extraction)

---

## ?? SUMMARY

**STEP 2 is CODE-COMPLETE** ?
- Header file: ? Ready
- Implementation: ? Ready  
- CMakeLists: ? Updated
- Just needs: OpenCV to compile

**Once OpenCV installed:**
- Run `build_with_opencv.bat`
- Get executable
- STEP 2 done!

---

**What would you like to do?**

1. ? Install OpenCV (has admin)
2. ? Get pre-built OpenCV (no admin)
3. ? Skip to STEP 3 (continue coding)
4. ? Something else

