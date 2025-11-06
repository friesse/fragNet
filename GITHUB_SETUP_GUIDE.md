# GitHub Repository Setup Guide

## Current Status

✅ **Build workflow created** - `.github/workflows/build.yml`  
✅ **Cryptopp fixed** - System package detection working  
❌ **Not pushed to GitHub** - Repository not initialized

## The Error You're Seeing

```
CMake Error at gc_server/CMakeLists.txt:50 (add_subdirectory):
add_subdirectory given source "gc_server" which is not an existing
directory.
```

**Why:** GitHub Actions can't find `gc_server/` and `steamworks/` because they're not in the repository yet.

---

## Solution: Push Your Project to GitHub

### Step 1: Create GitHub Repository

1. Go to https://github.com/new
2. **Repository name:** `gc-server` (or whatever you prefer)
3. **Description:** "Game Coordinator Server for ClassicCounter"
4. **Visibility:** Private (recommended) or Public
5. ❌ **DO NOT** initialize with README, .gitignore, or license
6. Click **"Create repository"**

### Step 2: Initialize Local Repository

Your git repo is already initialized! Now add and commit all files:

```bash
cd game-coordinator-server/gc-server-main

# Add all files
git add .

# Commit
git commit -m "Initial commit: GC Server with GitHub Actions"
```

### Step 3: Link to GitHub

Replace `YOUR_USERNAME` with your GitHub username:

```bash
# Add remote
git remote add origin https://github.com/YOUR_USERNAME/gc-server.git

# Rename branch to main (if needed)
git branch -M main

# Push to GitHub
git push -u origin main
```

### Step 4: Watch the Build

1. Go to your GitHub repository
2. Click **"Actions"** tab at the top
3. You should see your workflow running automatically!
4. Click on the running workflow to watch progress

---

## Expected GitHub Actions Flow

Once pushed, GitHub Actions will:

1. ✅ Checkout your code (including `gc_server/` and `steamworks/`)
2. ✅ Install dependencies (libcrypto++-dev, mariadb, etc.)
3. ✅ Find system cryptopp: 8.9
4. ✅ Configure CMake
5. ✅ Build gc-server for Linux and Windows
6. ✅ Upload artifacts

---

## What Gets Committed

All essential source files:
- ✅ `gc_server/` - Server source code
- ✅ `steamworks/` - Steam SDK files
- ✅ `protobufs/` - Protocol buffer definitions
- ✅ `items/` - Item schema files
- ✅ `.github/workflows/` - Build automation
- ✅ CMake files and build scripts
- ✅ README and documentation

**NOT committed** (per `.gitignore`):
- ❌ `build/` directories
- ❌ IDE files (`.vs/`, `.idea/`)
- ❌ CMake cache files

---

## Complete Command Reference

```bash
# Navigate to project
cd c:\fragmount(1)\fragmount\game-coordinator-server\gc-server-main

# Stage all files
git add .

# Commit with message
git commit -m "Initial commit: GC Server with GitHub Actions"

# Add your GitHub remote (replace YOUR_USERNAME and YOUR_REPO)
git remote add origin https://github.com/YOUR_USERNAME/YOUR_REPO.git

# Push to GitHub
git branch -M main
git push -u origin main
```

---

## Alternative: Use Existing Repository

If you already have a GitHub repository for this project:

```bash
# Add remote to existing repo
git remote add origin https://github.com/YOUR_USERNAME/EXISTING_REPO.git

# Pull existing commits (if any)
git pull origin main --allow-unrelated-histories

# Push your changes
git push -u origin main
```

---

## Verify Everything is Pushed

After pushing, check your GitHub repository web page:

**Should see:**
- ✅ `gc_server/` folder
- ✅ `steamworks/` folder
- ✅ `.github/workflows/build.yml`
- ✅ All source files

**Actions tab should show:**
- ✅ Workflow running or completed
- ✅ Green checkmark = success
- ✅ Artifacts available for download

---

## Troubleshooting

### "Permission denied (publickey)"

**Solution:** Use HTTPS or set up SSH keys

```bash
# Use HTTPS instead
git remote set-url origin https://github.com/YOUR_USERNAME/YOUR_REPO.git

# Try pushing again
git push -u origin main
```

### "Repository not found"

**Solution:** Make sure:
1. Repository exists on GitHub
2. Username/repo name is correct in URL
3. You have access to the repository

### "Large files warning"

**Solution:** If `steamworks/` contains large SDK files:

```bash
# Install Git LFS
git lfs install

# Track large files
git lfs track "steamworks/sdk/*.lib"
git lfs track "steamworks/sdk/*.dll"

# Commit LFS config
git add .gitattributes
git commit -m "Add Git LFS for large files"
git push
```

---

## What Happens Next

After successful push:

1. **GitHub Actions triggers automatically**
2. **Build runs on Linux and Windows**
3. **Artifacts created** (gc-server binaries)
4. **Download from Actions tab**
5. **Deploy to your VPS!**

---

## Quick Start (Copy-Paste)

```bash
# 1. Navigate to project
cd c:\fragmount(1)\fragmount\game-coordinator-server\gc-server-main

# 2. Add and commit
git add .
git commit -m "Initial commit: GC Server with GitHub Actions"

# 3. Add remote (CHANGE THIS!)
git remote add origin https://github.com/YOUR_USERNAME/gc-server.git

# 4. Push
git branch -M main
git push -u origin main

# 5. Open GitHub Actions
# Visit: https://github.com/YOUR_USERNAME/gc-server/actions
```

---

**Once you push, the cryptopp error will be resolved because all source files will be available to GitHub Actions!** ✅
