# Protocol Synchronization Notice

These protocol headers are **copies** from the authoritative source:
**https://github.com/dp-web4/Pack-Controller-EEPROM/tree/main/protocols**

## Keeping in Sync

When protocol definitions change in the Pack-Controller-EEPROM repository:

1. Update the authoritative version in Pack-Controller-EEPROM
2. Run `./scripts/sync_protocols.sh` in the ModuleCPU repository
3. Verify no breaking changes to ModuleCPU code
4. Check PROTOCOL_VERSION compatibility if defined
5. Commit with message: "sync: Update protocol definitions from Pack-Controller"

## Version Tracking

**Last synced from Pack-Controller-EEPROM**:
- Commit: 1aa6a9e
- Date: 2025-10-18 23:28:47 UTC

## Authoritative Source

The single source of truth for all ModBatt CAN protocol definitions is:
`Pack-Controller-EEPROM/protocols/`

Do NOT modify these files in ModuleCPU. All changes must be made in the
authoritative source and synced using this script.

## Building Standalone

If you are building ModuleCPU without having Pack-Controller-EEPROM cloned,
the protocol headers are already included in this repository. You do not need
to run the sync script unless you are actively developing protocol changes.
