#include "syscall.h"
#include "os-mm.h"
#include "libmem.h"
#include "queue.h"
#include "common.h"
#include "mm.h" 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



void extract_filename(char *path, char *output) {
    char *last_slash = strrchr(path, '/');
    if (last_slash) {
        strcpy(output, last_slash + 1);
    } else {
        strcpy(output, path);
    }
}

void get_proc_name(struct pcb_t *caller, int rgid, char *name) {
    int i = 0;
    BYTE data;
    int read_success = 0;

    struct vm_rg_struct *currg = &caller->mm->symrgtbl[rgid];
    if (currg->rg_start != 0 || currg->rg_end != 0) {
        int rg_start = currg->rg_start;
        while (i < 100) {
            if (MEMPHY_read(caller->krnl->mram, rg_start + i, &data) != 0) break;
            if (data != 0 && data != 255) read_success = 1;
            if (data == 0 || data == 255) { 
                name[i] = '\0';
                break;
            }
            name[i] = (char)data;
            i++;
        }
    }
    name[i] = '\0';

    if (strlen(name) == 0 || read_success == 0) {
        extract_filename(caller->path, name);
    }
}

void do_kill_proc(struct pcb_t *proc, struct pcb_t *caller) {
    printf("Process pid %d terminated successfully\n", proc->pid);
    if (proc == caller) {
        if (proc->code != NULL) {
            proc->pc = proc->code->size; 
        }
    } else {
        free_pcb_memph(proc);
        if(proc->mm) free(proc->mm);
        if(proc->code) free(proc->code);
        free(proc);
    }
}

int __sys_killall(struct krnl_t *krnl, uint32_t pid, struct sc_regs* regs)
{
    char name[100];
    char full_path[120] = "input/proc/"; 
    int rgid = regs->a1;
    struct pcb_t *caller = NULL;
    int killed_count = 0;
    struct pcb_t *victims[20]; 
    int victim_count = 0;

    if (krnl->running_list) 
    {
        for (int i=0; i<krnl->running_list->size; i++) 
        {
            if (krnl->running_list->proc[i]->pid == pid) 
            {
                caller = krnl->running_list->proc[i];
                break;
            }
        }
    }
    if (!caller) return -1;

    get_proc_name(caller, rgid, name);
    printf("The procname retrieved from memregionid %d is \"%s\"\n", rgid, name);

    if (strlen(name) == 0) return 0;
    
    strcpy(full_path, "input/proc/");
    strcat(full_path, name);

    
#ifdef MLQ_SCHED
    for (int i = 0; i < MAX_PRIO; i++) {
        struct queue_t *q = &krnl->mlq_ready_queue[i];
        for (int j = 0; j < q->size; j++) {
            struct pcb_t *proc = q->proc[j];
            if (proc && strcmp(proc->path, full_path) == 0) {
                purgequeue(q, proc);
                int exists = 0;
                for(int k=0; k<victim_count; k++) if(victims[k] == proc) exists=1;
                if(!exists) victims[victim_count++] = proc;
                j--; 
            }
        }
    }
#endif
    if (krnl->running_list) {
        for (int i = 0; i < krnl->running_list->size; i++) {
            struct pcb_t *proc = krnl->running_list->proc[i];
            if (proc && strcmp(proc->path, full_path) == 0) {
                if (proc != caller) {
                    purgequeue(krnl->running_list, proc);
                    i--;
                }
                int exists = 0;
                for(int k=0; k<victim_count; k++) if(victims[k] == proc) exists=1;
                if(!exists) victims[victim_count++] = proc;
            }
        }
    }

    int caller_is_dead = 0;
    for (int i = 0; i < victim_count; i++) {
        struct pcb_t *proc = victims[i];
        if (proc == caller) caller_is_dead = 1;
        
        do_kill_proc(proc, caller);
        killed_count++;
    }

    if (caller_is_dead == 0) {
        libfree(caller, rgid);
    }

    return killed_count;
}