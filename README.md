# KMenu Modern Search Integration (Classic Menu)

A major redesign of the search experience for the TDE Classic Menu (KMenu). 
This integration ports the instant-filtering search logic to the classic Trinity panel menu.

## Features
*   **Instant Filtering**: Replaces the legacy "path highlighting" logic with an instant, flat view of search results as you type.
*   **Bottom-Anchored Search Bar**: Strategic relocation of the search bar to the bottom of the menu for better ergonomics and modern aesthetics.
*   **Refined Lifecycle**: 
    *   **Contextual ESC**: Intelligently clears the search query on first press and closes the menu only when empty.
    *   **Auto-Reset**: Automatically restores the full menu tree and clears the query after an application is launched or the menu is dismissed.

## Technical Implementation
This enhancement utilizes advanced TQt/TDE widget management techniques:

### 1. Robust Widget Persistence
To maintain the search bar at the bottom during dynamic menu re-initialization (which typically wipes all children), the implementation uses a **Detachment/Re-attachment pattern**:
- **Detachment**: The search bar container is reparented to `NULL` during the base class `initialize()` wipe.
- **Persistence**: Re-insertion into the primary menu occurs immediately after initialization.
- **Visibility**: Explicit Z-order management (`raise()`) ensures the search bar is always anchored above filtered results.

### 2. Flat Search Mode State
The introduction of a `m_inFlatSearchMode` state machine allows the menu to seamlessly toggle between the traditional hierarchical tree view and the modern flat result view without reloading resources.

## Integration & Compilation

Compatible with `tdebase-trinity` 14.1.x series.

### 1. Automatic Integration
Deploy the files into your local source tree:
```bash
./integrate.sh /path/to/your/tdebase-trinity-root
```

### 2. Compilation
Re-compile the Kicker component:
```bash
cd /path/to/your/tdebase-trinity-root/build
make -j$(nproc) kicker
sudo make install
```

## Source Bundle Contents
- `src/`: Optimized source files (`k_mnu.cpp`, `k_mnu.h`).
- `integrate.sh`: Deployment script for rapid porting between TDE versions.
- `kmenu_modern_search.patch`: Unified diff for manual application if needed.

