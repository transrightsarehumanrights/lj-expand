#include "lj_expand_globals.h"

#include <stdlib.h>
#include <string.h>

static LJEGlobalState* lje_global_state = NULL;

LJEGlobalState* lje_get_global_state() {
    if (!lje_global_state) {
        lje_global_state = (LJEGlobalState*)malloc(sizeof(LJEGlobalState));
        memset(lje_global_state, 0, sizeof(LJEGlobalState));
    }

    return lje_global_state;
}

void lje_insert_spoof_record(GCfunc* spoof, GCfunc* target) {
    LJESpoofRecord* newRecord = (LJESpoofRecord*)malloc(sizeof(LJESpoofRecord));
    newRecord->spoof = spoof;
    newRecord->target = target;
    newRecord->next = LJEG()->spoof_record_root.next;
    LJEG()->spoof_record_root.next = newRecord;
}

GCfunc* lje_find_spoof_by_target(GCfunc* target) {
    for (LJESpoofRecord* current = LJEG()->spoof_record_root.next; current != NULL; current = current->next) {
        if (current->target == target) {
            return current->spoof;
        }
    }

    return NULL;
}

void lje_remove_spoof_record_by_spoof(GCfunc* spoof) {
    LJESpoofRecord* current = &LJEG()->spoof_record_root;
    while (current->next) {
        if (current->next->spoof == spoof) {
            LJESpoofRecord* toDelete = current->next;
            current->next = toDelete->next;
            free(toDelete);
            return;
        }

        current = current->next;
    }
}

void lje_clear_spoof_records() {
    LJESpoofRecord* current = LJEG()->spoof_record_root.next;
    while (current) {
        LJESpoofRecord* toDelete = current;
        current = current->next;
        free(toDelete);
    }

    LJEG()->spoof_record_root.next = NULL;
}