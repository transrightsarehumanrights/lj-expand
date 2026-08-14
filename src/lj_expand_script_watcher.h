#pragma once

#include "lj_expand_script.h"

typedef struct LJEScriptWatcher LJEScriptWatcher;

// Create a new script watcher instance
LJEScriptWatcher* lje_watcher_create(void);

// Add a script folder to watch for changes
// Returns 0 on success, -1 on failure
int lje_watcher_add_script(LJEScriptWatcher* watcher, LJEScript* script);

// Start the watcher thread
// Returns 0 on success, -1 on failure
int lje_watcher_start(LJEScriptWatcher* watcher);

// Get number of scripts pending reload
size_t lje_watcher_reload_count(LJEScriptWatcher* watcher);

// Pop the next script to reload (returns NULL if none pending)
LJEScript* lje_watcher_pop_reload(LJEScriptWatcher* watcher);

// Stop the watcher and free all resources
void lje_watcher_destroy(LJEScriptWatcher* watcher);