#include "syscall.h"
#include "os-mm.h"
#include "libmem.h"
#include "queue.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void get_proc_name(struct pcb_t *caller, int rgid, char *name) {
    int i = 0;
    BYTE data;
    while (1) {
        if (__read(caller, 0, rgid, i, &data) != 0) break;
        if (data == 0) { 
            name[i] = '\0';
            break;
        }
        name[i] = (char)data;
        i++;
        if (i >= 99) {
            name[i] = '\0';
            break;
        }
    }
}

int __sys_killall(struct krnl_t *krnl, uint32_t pid, struct sc_regs* regs)
{
    char name[100];
    char full_path[120] = "input/proc/"; 
    int rgid = regs->a1;
    struct pcb_t *caller = NULL;
    int killed_count = 0;
    if (krnl->running_list) {
        for (int i=0; i<krnl->running_list->size; i++) {
            if (krnl->running_list->proc[i]->pid == pid) {
                caller = krnl->running_list->proc[i];
                break;
            }
        }
    }
    if (!caller) return -1;
    get_proc_name(caller, rgid, name);
    if (strlen(name) == 0) return 0;
    strcat(full_path, name);

#ifdef MLQ_SCHED
    for (int i = 0; i < MAX_PRIO; i++) {
        struct queue_t *q = &krnl->mlq_ready_queue[i];
        for (int j = 0; j < q->size; j++) {
            struct pcb_t *proc = q->proc[j];
            
            if (proc && strcmp(proc->path, full_path) == 0 && proc->pid != pid) {
                purgequeue(q, proc);
                printf("Killall: Terminated process PID %d (%s)\n", proc->pid, proc->path);
                free_pcb_memph(proc);
                if(proc->mm) free(proc->mm);
                if(proc->code) free(proc->code);
                free(proc);
                killed_count++;
                j--; 
            }
        }
    }
#else
    struct queue_t *q = krnl->ready_queue;
    if (q) {
        for (int j = 0; j < q->size; j++) {
            struct pcb_t *proc = q->proc[j];
            if (proc && strcmp(proc->path, full_path) == 0 && proc->pid != pid) {
                
                purgequeue(q, proc);

                /* [LOGIC KILL ĐƯỢC ĐƯA VÀO ĐÂY] */
                printf("Killall: Terminated process PID %d (%s)\n", proc->pid, proc->path);
                free_pcb_memph(proc);
                if(proc->mm) free(proc->mm);
                if(proc->code) free(proc->code);
                free(proc);

                killed_count++;
                j--; 
            }
        }
    }
#endif

    return killed_count;
}