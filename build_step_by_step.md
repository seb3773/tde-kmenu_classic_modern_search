Step-by-step instructions for building and installing the TDE Kicker Menu Modern Search from source:

- TDE Versions: 14.1.1 or later (tested on 14.1.1)


- Getting the Source Code

First, check which version of TDE is installed on your system:

tde-config --version

Note the TDE version and then download TDE Base Source: You need the `tdebase` source code matching your installed
TDE version (Download from TDE Git)

--> Clone tdebase for your TDE version (replace 14.1.1 with your version)
git clone https://mirror.git.trinitydesktop.org/gitea/TDE/tdebase.git -b r14.1.1 tdebase-trinity-14.1.1

Then, clone the kicker modern search repository to a temporary location:
cd /tmp
git clone https://github.com/seb3773/tde-kicker-menu-modern-search

Next, copy the modified Kicker files into TDE Base: (let's assume you cloned tdebase in ~/src/)

The install.sh script does this automatically:

cd /tmp/tde-kicker-menu-modern-search
./install.sh ~/src/tdebase-trinity-14.1.1

Or you can copy files manually:

cp src/kicker/kicker/ui/k_mnu.cpp    ~/src/tdebase-trinity-14.1.1/kicker/kicker/ui/
cp src/kicker/kicker/ui/k_mnu.h      ~/src/tdebase-trinity-14.1.1/kicker/kicker/ui/
cp src/kicker/kicker/ui/popupmenutitle.cpp ~/src/tdebase-trinity-14.1.1/kicker/kicker/ui/
cp src/kicker/kicker/ui/popupmenutitle.h   ~/src/tdebase-trinity-14.1.1/kicker/kicker/ui/
cp src/kicker/kicker/CMakeLists.txt   ~/src/tdebase-trinity-14.1.1/kicker/kicker/
cp src/kicker/libkicker/CMakeLists.txt ~/src/tdebase-trinity-14.1.1/kicker/libkicker/
mkdir -p ~/src/tdebase-trinity-14.1.1/kicker/data/icons
cp src/kicker/data/icons/hi16-app-search_empty.png ~/src/tdebase-trinity-14.1.1/kicker/data/icons/

Then verify the files are in place:
ls ~/src/tdebase-trinity-14.1.1/kicker/kicker/ui/k_mnu.*

You should see `k_mnu.cpp` and `k_mnu.h`.



- IMPORTANT: Patch kickerSettings.kcfg (MANDATORY for ALL TDE versions)

**This step is MANDATORY for ALL TDE versions (14.1.1, 14.1.5, and later).** The modern search menu 
requires two additional configuration entries (`ClassicKMenuOpacity` and `ClassicKMenuBackgroundColor`) 
that must be added to your tdebase's `kickerSettings.kcfg` file.

Run the patch script from the kicker directory:

cd ~/src/tdebase-trinity-14.1.1/kicker
./patch_kickerSettings.sh

Expected output:

```
Patching libkicker/kickerSettings.kcfg...
✓ Created backup: libkicker/kickerSettings.kcfg.backup
✓ Successfully patched libkicker/kickerSettings.kcfg
  Added ClassicKMenuOpacity and ClassicKMenuBackgroundColor entries

Patch completed successfully!
You can now proceed with the compilation.
```

If the entries already exist, you'll see:

```
✓ kickerSettings.kcfg already contains the required entries
  No patching needed!
```

**Important:** The `kickerSettings.kcfg` file is NOT provided in this repository to avoid version conflicts.
You must always patch your existing tdebase file using this script.

  
- Prerequisites

Make sure you have:
- CMake >= 3.x
- GCC/G++ with C++11 support
- TDE development headers (libtqt-dev, tdelibs-dev, etc.)
- Gold linker (optional, for smaller binaries)
- sstrip (optional, for maximum binary compression)
- tde-cmake (required for TDE 14.1.5+)

Verify tools:

cmake --version
g++ --version
ld.gold --version
sstrip --version

The gold linker and sstrip are optional but recommended for production builds. If they are not available,
the build system will fall back to the standard linker and strip.


Now, all commands assume you are in the tdebase directory:

cd ~/src/tdebase-trinity-14.1.1


- Understanding the Build Architecture

Unlike simpler projects like compton-tde which can be built standalone with `cmake .`, the kicker module
depends on TDE-specific CMake macros (tde_add_library, tde_install_export, etc.). These macros are defined
in the parent tdebase CMake configuration.

This means the build is a TWO-STEP process:
1. **cmake** is run from the tdebase root (to load TDE macros) — this is fast, it only parses CMakeLists files
2. **make** is run from **build/kicker/** only — this compiles only kicker, not the entire tdebase

This is important: do NOT try `cmake ../../kicker` from a subdirectory — it will fail with
"Unknown CMake command tde_add_library".


Now, let's build. There are two build modes:


- Mode 1: Debug Build (with symbols, for development)

> Includes debug symbols and no optimizations. Useful for development and debugging with GDB.

cd ~/src/tdebase-trinity-14.1.1
mkdir -p build
cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX=/opt/trinity \
  -DBUILD_KICKER=ON \
  -DBUILD_ALL=OFF

cd kicker
make -j$(nproc)

Binary size will be large (~500KB+) due to debug symbols.


- Mode 2: Optimized Production Build (stripped, minimal size)

> Maximum optimization with aggressive flags. Binary size: ~3KB after sstrip.
> This is what I use for production release.

cd ~/src/tdebase-trinity-14.1.1
mkdir -p build
cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/opt/trinity \
  -DCMAKE_CXX_FLAGS="-g0 -O2 -Wl,--relax -Wl,-z,norelro -fstrict-aliasing -flto=auto -ffunction-sections -fdata-sections -fno-ident -fno-plt -fno-asynchronous-unwind-tables -fno-unwind-tables -fomit-frame-pointer -fno-stack-protector -fno-math-errno -ffast-math -fvisibility=hidden -fmerge-all-constants -fuse-ld=gold -Wl,--gc-sections,--build-id=none,--as-needed,--strip-all,-O1,--icf=all,--compress-debug-sections=zlib -s -Wno-deprecated-declarations" \
  -DBUILD_KICKER=ON \
  -DBUILD_ALL=OFF \
  -DWITH_TDEHWLIB=ON

cd kicker
make -j2


During CMake configuration, you should see something like:

-- The C compiler identification is GNU 12.2.0
-- The CXX compiler identification is GNU 12.2.0
...
-- found 'TDE', version 14.1.1
...
-- Configuring done
-- Generating done
-- Build files have been written to: /home/user/src/tdebase-trinity-14.1.1/build


During compilation (from build/kicker/), you should see something like:

[  0%] Built target kicker+kicker+core+panelextension.kidl
[  0%] Built target kicker+kicker+core+kicker.kidl
[  2%] Building CXX object kicker/libkicker/CMakeFiles/kickermain-shared.dir/appletinfo.cpp.o
...
[ 78%] Linking CXX static library libkicker_ui.a
[100%] Built target kicker_ui-static
[100%] Linking CXX shared library libtdeinit_kicker.so
[100%] Built target tdeinit_kicker-shared
[100%] Built target kicker

Compilation takes around 2 minutes on a modest machine.


After a successful build, the key generated files are:

```
build/kicker/
├── kicker/
│   ├── kicker                          # Main executable binary
│   ├── libtdeinit_kicker.so            # Kicker init library
│   └── ...
├── libkicker/
│   ├── libkickermain.so.1.0.0          # Core library
│   └── ...
├── applets/                            # Panel applets (clock, systray, etc.)
├── extensions/                         # Panel extensions (taskbar, kasbar, etc.)
├── menuext/                            # Menu extensions
└── ...
```


* Installing

To install the kicker system-wide (requires root privileges):

Make sure you're in the build/kicker directory:
cd ~/src/tdebase-trinity-14.1.1/build/kicker

sudo make install

This will:
- Copy binaries to /opt/trinity/bin/
- Copy libraries to /opt/trinity/lib/
- Copy applets and extensions to /opt/trinity/lib/trinity/
- Automatically run **sstrip** on the kicker binary (if available) to compress it to ~3KB
- Automatically run **sstrip** on libkickermain.so (if available)

Expected install output (for optimized build):

-- Installing: /opt/trinity/lib/libkickermain.so.1.0.0
-- Performing post-install stripping for kickermain...
FOUND sstrip, optimizing...
...
-- Installing: /opt/trinity/bin/kicker
-- Performing post-install stripping...
FOUND sstrip, optimizing...

Verify the installation:

ls -l /opt/trinity/bin/kicker

For an optimized build with sstrip:
-rwxr-xr-x 1 root root 3000 Feb 11 21:56 /opt/trinity/bin/kicker

If sstrip is not available, the binary will be around 8-12KB (still small, stripped with standard strip).


To test without installing system-wide, you can restart kicker from the build directory:

dcop kicker kicker restart


** Packaging

Build the binary first of course, then run the Packaging Script:

cd ~/src/tdebase-trinity-14.1.1/kicker
chmod +x ./create_deb.sh
./create_deb.sh

Expected output:

```
Creating .deb package for tde-kicker-q4win10...
  Architecture: amd64
Installing to intermediate directory...
...
Stripping binaries (MAXIMUM optimized)...
Creating control file...
Building package...
dpkg-deb: building package 'tde-kicker-q4win10' in 'tde-kicker-q4win10_14.1.1_amd64.deb'.
Success! Package created: tde-kicker-q4win10_14.1.1_amd64.deb
-rw-r--r-- 1 user user 522K Feb 11 21:57 tde-kicker-q4win10_14.1.1_amd64.deb
```

(warning about "unusual owner or group" is normal when building packages as a non-root user, this can be safely
ignored.)

The created package includes:
- **Binary**: `/opt/trinity/bin/kicker` (~3KB)
- **Libraries**: `libkickermain.so`, `libtdeinit_kicker.so`
- **Modern Interactive Sidebar**: Windows 10-inspired buttons for **Switch User**, **Documents**, **Pictures**, **Settings**, and **Log Out**.
- **Interactive Menu Titles**: Toggle Recent/Frequent, Direct Shutdown, and User Config.
- **Panel applets**: clock, systray, launcher, etc.
- **Menu extensions**: recent docs, konsole, kate, etc.
- **Icons**: search_empty.png (transparent padding icon), menu-*.png (sidebar icons)
- **Package name**: `tde-kicker-classicx-q4win10`
- **Version**: 14.1.1 (dynamically detected)
- **Architecture**: Automatically detected


To install the .deb package:

sudo dpkg -i tde-kicker-q4win10_14.1.1_amd64.deb


---

## Troubleshooting (some issues I experienced myself)


**** CMake Error - "Unknown CMake command 'tde_add_library'"
Error message:

CMake Error at CMakeLists.txt (tde_add_library):
  Unknown CMake command "tde_add_library".

--> You tried to run `cmake` directly on the kicker subdirectory (e.g. `cmake ../../kicker`). This does not work
because kicker depends on TDE-specific CMake macros that are only loaded from the root tdebase CMakeLists.txt.

The correct approach: always run `cmake ..` from the `build/` directory at the tdebase root level
(with `-DBUILD_KICKER=ON -DBUILD_ALL=OFF`), then `cd kicker && make`.


**** CMake Error - "CMakeCache.txt directory is different"
Error message:

CMake Error: The current CMakeCache.txt directory /path/to/build/CMakeCache.txt
is different than the directory /other/path/build where CMakeCache.txt was created.

--> You moved or renamed the build directory. Clean the build artifacts before running CMake:

cd ~/src/tdebase-trinity-14.1.1
rm -rf build
mkdir build && cd build

And run cmake again.


**** CMake Error - "include could not find requested file: TDEVersion" (TDE 14.1.5+)

--> TDE 14.1.5 introduced new CMake modules that are not part of the standard tdebase source.
You must install the `tde-cmake` package:

sudo apt install tde-cmake

This will install `TDEVersion.cmake` and other essential modules to your system.


**** "make: *** No targets specified and no makefile found"

--> You're in the wrong directory or cmake hasn't been run yet:

cd ~/src/tdebase-trinity-14.1.1/build

Run cmake first:

cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/trinity \
  -DBUILD_KICKER=ON -DBUILD_ALL=OFF

Then build:

cd kicker
make -j$(nproc)


**** Build takes "forever" (more than 5 minutes)

--> You are probably compiling the entire tdebase instead of just kicker. Make sure:

1. You configured with `-DBUILD_KICKER=ON -DBUILD_ALL=OFF`
2. You are running `make` from `build/kicker/`, NOT from `build/`

Running `make` from `build/` will try to build everything. Running from `build/kicker/` only compiles kicker.


**** Binary size is 12KB instead of 3KB

--> You are missing the production optimization flags. The standard `-O2 -s` is not sufficient.
The full flag set is critical for reaching ~3KB:

Key flags that reduce binary size:
- `-ffunction-sections -fdata-sections` + `-Wl,--gc-sections` : dead code elimination
- `-fno-unwind-tables -fno-asynchronous-unwind-tables` : remove exception unwinding info
- `-fvisibility=hidden` : hide internal symbols
- `--icf=all` : identical code folding (requires gold linker)
- `sstrip` : removes ELF sections not needed at runtime

Without these flags, `sstrip` alone cannot compensate and the binary will remain ~12KB.


**** sstrip not found / not applied

--> sstrip is NOT included in standard distributions. You may need to build it from source:

git clone https://github.com/nicktehrany/sstrip
cd sstrip
make
sudo cp sstrip /usr/local/bin/

If sstrip is not available, the CMakeLists.txt will automatically fall back to `strip --strip-all`,
which produces a slightly larger binary (~8-12KB).


**** Compilation errors: TQT_SIGNAL, TQT_SLOT, tqdrawPrimitive (TDE 14.1.5+)

--> TDE 14.1.5 cleaned up some older TQt API prefixes. Specifically:
- `TQT_SIGNAL` and `TQT_SLOT` were renamed to `TQ_SIGNAL` and `TQ_SLOT`
- `tqdrawPrimitive` (method) was renamed to `drawPrimitive`

The provided Modern Search source files (`k_mnu.h`, `k_new_mnu.h`, `popupmenutitle.h`) include
compatibility guards that automatically map these back for smooth compilation on both 14.1.1 and 14.1.5.
No manual source modification is required if you are using the latest version of these files.



**** CRITICAL: Kicker loads OLD version despite successful build & install

Symptoms:
- `sudo make install` succeeds and installs to `/opt/trinity/`
- But the running kicker shows OLD behavior (missing features, old layout)
- You are 100% sure the source code is correct

--> The system may have a **second copy** of kicker binaries in `/usr/local/` from a previous
installation. The running process loads from `/usr/local/` because it takes priority over
`/opt/trinity/` in the dynamic linker search path.

How to diagnose:

```
# Check which binary the running kicker is actually using:
KPID=$(pgrep -x kicker | head -1)
cat /proc/$KPID/maps | grep -i kicker
```

If you see `/usr/local/lib/libtdeinit_kicker.so` instead of `/opt/trinity/lib/libtdeinit_kicker.so`,
that's the problem. The running kicker is loading stale binaries from `/usr/local/`.

How to fix:

```
# Remove ALL kicker binaries from /usr/local/ (they should NOT be there):
sudo rm -f /usr/local/bin/kicker
sudo rm -f /usr/local/lib/libtdeinit_kicker.so
sudo rm -f /usr/local/lib/libtdeinit_kicker.la
sudo rm -f /usr/local/lib/libkickermain.so
sudo rm -f /usr/local/lib/libkickermain.so.1
sudo rm -f /usr/local/lib/libkickermain.so.1.0.0
sudo rm -f /usr/local/lib/libkickermain.la
sudo ldconfig

# Then reinstall properly:
cd ~/src/tdebase-trinity-14.1.1/build/kicker
sudo make install

# Restart kicker:
dcop kicker kicker restart
```

The key library that contains ALL the UI code (menus, sidebar, search, sessions) is
`libtdeinit_kicker.so`, NOT `libkickermain.so`. Always verify this file is up to date.

Prevention: after ANY `cmake` reconfiguration, always check that `CMAKE_INSTALL_PREFIX`
is set to `/opt/trinity` and that no stale binaries exist in `/usr/local/`:

```
ls /usr/local/bin/kicker /usr/local/lib/libtdeinit_kicker* /usr/local/lib/libkickermain* 2>/dev/null
```

If any files are listed, remove them.


- cmake options:

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Release/Debug | (none) | Build type: Release for production, Debug for development |
| `CMAKE_INSTALL_PREFIX` | path | /usr/local | Installation prefix (use `/opt/trinity` for TDE) |
| `CMAKE_CXX_FLAGS` | string | (default) | Custom compiler/linker flags (use full set for production) |
| `BUILD_KICKER` | ON/OFF | OFF | Build the kicker panel module |
| `BUILD_ALL` | ON/OFF | ON | Build all tdebase modules (set OFF to build only kicker) |
