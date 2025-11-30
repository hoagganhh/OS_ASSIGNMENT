#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
        if (q == NULL)
                return 1;
        return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: put a new process to queue [q] */
        if(q == NULL || proc == NULL){
                return;
        }
        if (q->size < MAX_QUEUE_SIZE) {
                q->proc[q->size] = proc;
                q->size++;
        }
        else {
                fprintf(stderr, "Queue is full, cannot enqueue process.\n");
        }
}

struct pcb_t *dequeue(struct queue_t *q)
{
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */

        if(q != NULL && q->size > 0){ //if queue not NULL and empty
                struct pcb_t *proc = NULL;
                int remove_index = -1;
#ifdef MLQ_SCHED //if using MLQ
                proc = q->proc[0]; //because all process prio are equal in a queue, return procces at head of queue
                remove_index = 0;
#else //if not using MLQ
                int highest_prio = 2147483640;
                for(int i = 0; i < q->size; i++){ //loop through all process in queue
                        if(q->proc[i]->priority < highest_prio){ //if prio higher than previous prio
                                highest_prio = q->proc[i]->priority; //assign current highest prio
                                remove_index = i; //assign current highest prio index
                        }
                }

                if(remove_index != -1){
                        proc = q->proc[remove_index];
                }
#endif

                if(proc != NULL){
                        for(int i = remove_index + 1; i < q->size; i++){
                                q->proc[i - 1] = q->proc[i];
                        }
                        q->size--;
                        q->proc[q->size] = NULL;
                }

                return proc;
        }

	return NULL;
}

struct pcb_t *purgequeue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: remove a specific item from queue
         * */

        if(q != NULL && proc != NULL && !empty(q)){
                int index = -1;
                struct pcb_t *found_proc = NULL;

                for(int i = 0; i < q->size; i++){
                        if(q->proc[i]->pid == proc->pid){
                                index = i;
                                found_proc = q->proc[i];
                                break;
                        }
                }

                if(index >= 0 && found_proc != NULL){
                        for(int i = index + 1; i < q->size; i++){
                                q->proc[i - 1] = q->proc[i];
                        }
                        q->size--;
                        q->proc[q->size] = NULL;

                        return found_proc;
                }
        }
        return NULL;
}