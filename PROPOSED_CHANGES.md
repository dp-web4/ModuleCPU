# Persistent Module Records with Registration Status

## Better Solution: Persistent Module Records
Instead of removing modules from the array, keep them and track registration status. This handles intermittent modules gracefully.

## Implementation

### 1. Add registration status to module structure
In `/mnt/c/projects/ai-agents/Pack-Controller-EEPROM/Core/Inc/bms.h`:
```c
typedef struct {
  // ... existing fields ...
  bool        isRegistered;     // Module currently registered
  // ... rest of fields ...
}batteryModule;
```

### 2. Modify deregistration to just mark as unregistered
In PCU_Tasks timeout handling (~line 357):
```c
// Instead of removing from array:
// Mark module as deregistered
module[index].isRegistered = false;

// Don't shift array or change pack.moduleCount
// Continue to next module without adjusting index
```

### 3. Update registration logic
In MCU_RegisterModule (~line 1040):
```c
// Check if module already exists (registered or not)
moduleIndex = MAX_MODULES_PER_PACK; // Invalid index
for(index = 0; index < MAX_MODULES_PER_PACK; index++){
    if(module[index].uniqueId == announcement.moduleUniqueId){
        moduleIndex = index;
        break;
    }
}

if(moduleIndex < MAX_MODULES_PER_PACK){
    // Existing module - just mark as registered
    module[moduleIndex].isRegistered = true;
    module[moduleIndex].faultCode.commsError = 0;
    // ... reset timeouts etc
    
    if(debugMessages & DBG_MSG_ANNOUNCE){
        sprintf(tempBuffer,"MCU INFO - Module RE-REGISTERED: Index=%d, ID=%02x, UID=%08x",
                moduleIndex, module[moduleIndex].moduleId, announcement.moduleUniqueId);
        serialOut(tempBuffer);
    }
}
else {
    // New module - find first empty slot
    for(index = 0; index < MAX_MODULES_PER_PACK; index++){
        if(module[index].uniqueId == 0){  // Empty slot
            moduleIndex = index;
            break;
        }
    }
    
    if(moduleIndex < MAX_MODULES_PER_PACK){
        // Initialize new module
        module[moduleIndex].moduleId = moduleIndex + 1;  // ID = index + 1
        module[moduleIndex].uniqueId = announcement.moduleUniqueId;
        module[moduleIndex].isRegistered = true;
        // ... rest of initialization
        
        // Update pack.moduleCount to highest registered module
        pack.moduleCount = 0;
        for(int i = 0; i < MAX_MODULES_PER_PACK; i++){
            if(module[i].isRegistered && module[i].uniqueId != 0){
                pack.moduleCount++;
            }
        }
    }
}
```

### 4. Update all module iterations to check registration
Everywhere we iterate through modules:
```c
for(index = 0; index < MAX_MODULES_PER_PACK; index++){
    if(!module[index].isRegistered || module[index].uniqueId == 0) continue;
    // ... process module
}
```

### 5. Update module finding functions
```c
uint8_t MCU_ModuleIndexFromId(uint8_t moduleId){
    for(index = 0; index < MAX_MODULES_PER_PACK; index++){
        if(module[index].moduleId == moduleId && module[index].isRegistered)
            return index;
    }
    return MAX_MODULES_PER_PACK; // Not found
}
```

## Benefits
1. Module IDs never change once assigned
2. Handles intermittent connections gracefully
3. No array shifting complications
4. Module history is preserved
5. Can track metrics across connect/disconnect cycles
6. Simpler logic overall

## Module Counting
```c
// Add to pack structure:
uint8_t totalModules;    // Count of all modules ever seen (non-empty slots)
uint8_t activeModules;   // Count of currently registered modules

// Update counts whenever registration changes:
void MCU_UpdateModuleCounts(void){
    pack.totalModules = 0;
    pack.activeModules = 0;
    
    for(int i = 0; i < MAX_MODULES_PER_PACK; i++){
        if(module[i].uniqueId != 0){
            pack.totalModules++;
            if(module[i].isRegistered){
                pack.activeModules++;
            }
        }
    }
}
```

When reporting to host controller:
- Total modules = all modules that have ever connected (pack.totalModules)
- Active modules = currently registered modules (pack.activeModules)
- This helps diagnose intermittent connection issues

## Notes
- pack.moduleCount can be deprecated or repurposed
- Module slots are reused based on unique ID
- Empty slots have uniqueId = 0
- Host sees both "modules that exist" and "modules currently online"