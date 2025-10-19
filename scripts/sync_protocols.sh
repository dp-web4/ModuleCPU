#!/bin/bash
# sync_protocols.sh
# Copies protocol headers from Pack-Controller-EEPROM to ModuleCPU
#
# This script maintains a local copy of the CAN protocol definitions that are
# authoritatively maintained in the Pack-Controller-EEPROM repository.
#
# Usage: ./scripts/sync_protocols.sh

set -e  # Exit on error

PACK_PROTOCOLS="../Pack-Controller-EEPROM/protocols"
MODULE_PROTOCOLS="./protocols"

echo "=== ModuleCPU Protocol Sync ==="
echo ""

# Check if Pack-Controller-EEPROM protocols exist
if [ ! -d "$PACK_PROTOCOLS" ]; then
    echo "Error: Pack-Controller-EEPROM protocols not found at $PACK_PROTOCOLS"
    echo ""
    echo "This script expects Pack-Controller-EEPROM to be cloned at:"
    echo "  ../Pack-Controller-EEPROM"
    echo ""
    echo "If you're building ModuleCPU standalone, the protocol headers are already"
    echo "included in this repository. No sync needed."
    exit 1
fi

echo "Source: $PACK_PROTOCOLS"
echo "Target: $MODULE_PROTOCOLS"
echo ""

# Create protocols directory if it doesn't exist
mkdir -p "$MODULE_PROTOCOLS"

# Get git commit hash from Pack-Controller-EEPROM for tracking
PACK_COMMIT=""
if [ -d "../Pack-Controller-EEPROM/.git" ]; then
    cd ../Pack-Controller-EEPROM
    PACK_COMMIT=$(git rev-parse --short HEAD)
    cd - > /dev/null
fi

# Copy protocol files
echo "Copying protocol headers..."
cp "$PACK_PROTOCOLS/CAN_ID_ALL.h" "$MODULE_PROTOCOLS/"
cp "$PACK_PROTOCOLS/can_frm_mod.h" "$MODULE_PROTOCOLS/"
cp "$PACK_PROTOCOLS/README.md" "$MODULE_PROTOCOLS/"

echo "✓ CAN_ID_ALL.h"
echo "✓ can_frm_mod.h"
echo "✓ README.md"
echo ""

# Create sync notice
echo "Creating sync notice..."
cat > "$MODULE_PROTOCOLS/SYNC_NOTICE.md" << EOF
# Protocol Synchronization Notice

These protocol headers are **copies** from the authoritative source:
**https://github.com/dp-web4/Pack-Controller-EEPROM/tree/main/protocols**

## Keeping in Sync

When protocol definitions change in the Pack-Controller-EEPROM repository:

1. Update the authoritative version in Pack-Controller-EEPROM
2. Run \`./scripts/sync_protocols.sh\` in the ModuleCPU repository
3. Verify no breaking changes to ModuleCPU code
4. Check PROTOCOL_VERSION compatibility if defined
5. Commit with message: "sync: Update protocol definitions from Pack-Controller"

## Version Tracking

**Last synced from Pack-Controller-EEPROM**:
- Commit: ${PACK_COMMIT:-unknown}
- Date: $(date -u +"%Y-%m-%d %H:%M:%S UTC")

## Authoritative Source

The single source of truth for all ModBatt CAN protocol definitions is:
\`Pack-Controller-EEPROM/protocols/\`

Do NOT modify these files in ModuleCPU. All changes must be made in the
authoritative source and synced using this script.

## Building Standalone

If you are building ModuleCPU without having Pack-Controller-EEPROM cloned,
the protocol headers are already included in this repository. You do not need
to run the sync script unless you are actively developing protocol changes.
EOF

echo "✓ SYNC_NOTICE.md"
echo ""

echo "=== Protocol Sync Complete ==="
echo ""
echo "Files updated:"
echo "  - protocols/CAN_ID_ALL.h"
echo "  - protocols/can_frm_mod.h"
echo "  - protocols/README.md"
echo "  - protocols/SYNC_NOTICE.md"
echo ""

if [ -n "$PACK_COMMIT" ]; then
    echo "Synced from Pack-Controller-EEPROM commit: $PACK_COMMIT"
    echo ""
fi

echo "Next steps:"
echo "  1. Review changes: git diff protocols/"
echo "  2. Test build: (build in Atmel Studio or your build system)"
echo "  3. Commit: git add protocols/ && git commit -m 'sync: Update protocol definitions'"
echo ""
