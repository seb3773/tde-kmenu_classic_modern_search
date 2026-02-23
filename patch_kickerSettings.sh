#!/bin/bash
# Script to patch kickerSettings.kcfg for TDE 14.1.5 compatibility
# This adds the ClassicKMenuOpacity and ClassicKMenuBackgroundColor entries
# needed for the modern search menu to compile successfully

set -e

KCFG_FILE="libkicker/kickerSettings.kcfg"

# Check if we're in the right directory
if [ ! -f "$KCFG_FILE" ]; then
    echo "Error: $KCFG_FILE not found!"
    echo "Please run this script from the kicker directory"
    exit 1
fi

# Check if entries already exist
if grep -q "ClassicKMenuOpacity" "$KCFG_FILE" && grep -q "ClassicKMenuBackgroundColor" "$KCFG_FILE"; then
    echo "✓ kickerSettings.kcfg already contains the required entries"
    echo "  No patching needed!"
    exit 0
fi

echo "Patching $KCFG_FILE..."

# Backup the original file
cp "$KCFG_FILE" "${KCFG_FILE}.backup"
echo "✓ Created backup: ${KCFG_FILE}.backup"

# Add the two new entries after the <group name="KMenu" > line
sed -i '/<group name="KMenu" >/a\
\
   <entry name="ClassicKMenuOpacity" type="Int" >\
      <label>TDE Menu Opacity</label>\
      <default>100</default>\
      <min>0</min>\
      <max>100</max>\
      <whatsthis>Controls the level of pseudo-transparency applied to the TDE menu.</whatsthis>\
   </entry>\
\
   <entry name="ClassicKMenuBackgroundColor" type="Color" >\
      <label>TDE Menu Background Color</label>\
      <default code="true">TDEGlobalSettings::buttonBackground()</default>\
      <whatsthis>Controls the background color applied to the TDE menu.</whatsthis>\
   </entry>' "$KCFG_FILE"

# Verify the patch was applied
if grep -q "ClassicKMenuOpacity" "$KCFG_FILE" && grep -q "ClassicKMenuBackgroundColor" "$KCFG_FILE"; then
    echo "✓ Successfully patched $KCFG_FILE"
    echo "  Added ClassicKMenuOpacity and ClassicKMenuBackgroundColor entries"
else
    echo "✗ Failed to patch $KCFG_FILE"
    echo "  Restoring backup..."
    mv "${KCFG_FILE}.backup" "$KCFG_FILE"
    exit 1
fi

echo ""
echo "Patch completed successfully!"
echo "You can now proceed with the compilation."
