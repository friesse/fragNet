# GitHub Actions Build Fix - Cryptopp Error

## Problem
```
CMake Error at gc_server/CMakeLists.txt:70 (message):
  cryptopp library not found!
```

## Root Cause
The project uses `FetchContent` to download `cryptopp-cmake` from GitHub during the CMake configuration step. This can fail if:
1. Network connectivity issues
2. GitHub rate limiting
3. Missing system dependencies

## Solution Applied

### ✅ Updated `.github/workflows/build.yml`

**Added dependencies:**
```yaml
sudo apt-get install -y \
  libcrypto++-dev \   # System cryptopp library
  git                  # For FetchContent
```

**Added submodule checkout:**
```yaml
- uses: actions/checkout@v4
  with:
    submodules: recursive
```

**Split build steps:**
```yaml
- name: Configure CMake (Linux)
  run: |
    cmake .. -DCRYPTOPP_BUILD_TESTING=OFF \
             -DCRYPTOPP_INSTALL=OFF

- name: Build (Linux)
  run: |
    cmake --build . -j$(nproc)
```

## Alternative Solution (Use System Package)

If FetchContent continues to fail, modify `CMakeLists.txt` to use system cryptopp:

### Option 1: Find System Package First

Add this **before** the FetchContent section in `CMakeLists.txt`:

```cmake
# Try to find system cryptopp first
find_package(cryptopp QUIET)

if(NOT cryptopp_FOUND)
    # Fall back to FetchContent
    option(CRYPTOPP_BUILD_TESTING "Build library tests" OFF)
    option(CRYPTOPP_INSTALL "Generate the install target for this project." OFF)
    
    FetchContent_Declare(cryptopp-cmake 
        GIT_REPOSITORY https://github.com/abdes/cryptopp-cmake 
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(cryptopp-cmake)
endif()
```

### Option 2: Use PkgConfig

```cmake
# Use pkg-config to find cryptopp
find_package(PkgConfig REQUIRED)
pkg_check_modules(CRYPTOPP REQUIRED libcrypto++)

if(CRYPTOPP_FOUND)
    target_include_directories(gc-server PRIVATE ${CRYPTOPP_INCLUDE_DIRS})
    target_link_libraries(gc-server PRIVATE ${CRYPTOPP_LIBRARIES})
else()
    # Fall back to FetchContent
    # ... existing FetchContent code
endif()
```

## Test Locally

Before pushing, test the build locally:

```bash
# Install dependencies
sudo apt-get install libcrypto++-dev

# Configure
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . -j$(nproc)
```

## Verify Fix

After pushing:

1. Go to **Actions** tab on GitHub
2. Check the "Configure CMake (Linux)" step
3. Should see: "Found cryptopp" or successful FetchContent download
4. Build should complete successfully

## Current Status

✅ Workflow updated with libcrypto++-dev  
✅ Git submodules enabled  
✅ CMake options explicitly set  
⏳ Push to test

## Next Steps

1. **Commit and push** the updated workflow:
   ```bash
   git add .github/workflows/build.yml
   git commit -m "Fix: Add cryptopp dependency for GitHub Actions"
   git push
   ```

2. **Monitor the build** in Actions tab

3. **If still failing**, apply Alternative Solution above

---

**The workflow is now configured to install cryptopp from the Ubuntu package repository, which should resolve the build error.**
