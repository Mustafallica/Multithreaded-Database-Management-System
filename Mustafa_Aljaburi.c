#include <stdlib.h>
#include <stdio.h>
#include <zconf.h>
#include <pthread.h>
#include <semaphore.h>
#define ENTRY_COUNT 10
#define GROUP_COUNT 2


//linked list to store threads
struct thread {
    pthread_t data;
    struct thread *next;
};

//struct to store the summery to be printed at last
struct {
    pthread_mutex_t access;
    int request_count[GROUP_COUNT],group_wait_count,locked_pos_wait_count;    
} stats;

//struct to store the arguments to the request threads
struct request_data {
    int user, group, req_pos, req_time, req_duration;
};

//struct to represent a database entry
struct entry {
    pthread_mutex_t access;
    pthread_cond_t data_access;
    int used_by;
};

//current_group_lock, current_group, and current_group_cond are used to create a monitor to wait the low priority group
pthread_mutex_t current_group_lock;
pthread_cond_t current_group_cond;
int current_group;

//array to represent the database
struct entry database[ENTRY_COUNT];

void *req(void *_data);

int main(int argc, char *argv[]) {
    //Definitions, declarations and initializations
    struct thread *group_threads[GROUP_COUNT];
    struct thread *head[GROUP_COUNT];

    for (int i = 0; i < ENTRY_COUNT; i++) {
        pthread_mutex_init(&database[i].access, NULL);
        database[i].used_by = -1;
        pthread_cond_init(&database[i].data_access, NULL);
    }

    for (int i = 0; i < GROUP_COUNT; i++) {
        stats.request_count[i] = 0;
        group_threads[i] = NULL;
        head[i] = NULL;
    }
    pthread_mutex_init(&stats.access, NULL);
    stats.group_wait_count = 0;
    stats.locked_pos_wait_count = 0;

    pthread_mutex_init(&current_group_lock, NULL);
    pthread_cond_init(&current_group_cond, NULL);

    //Get the priority group
    scanf("%d\n", &current_group);
    current_group--; // 0 based indexing is used internally

    int user = 0;
    int req_time = 0;
    while (1) {
        struct request_data *data = malloc(sizeof(struct request_data));
        int req_time_from;
        if (scanf("%d %d %d %d\n", &data->group, &data->req_pos, &req_time_from, &data->req_duration) == EOF)
            break; //Reached the end of file

        data->user = user++;
        data->group--;   // 0 based indexing is used internally
        data->req_pos--; // 0 based indexing is used internally
        data->req_time = req_time += req_time_from;

        //        printf("User %d from Group %d requesting position %d at time %d for %d sec.\n", data->user+1, data->group+1, data->req_pos+1, data->req_time, data->req_duration);

        if (head[data->group] == NULL) { //The first access and initialization of the linked list is different from the subsequent accesses
            head[data->group] = malloc(sizeof(struct thread));
            group_threads[data->group] = head[data->group];
        } else {
            head[data->group]->next = malloc(sizeof(struct thread));
            head[data->group] = head[data->group]->next;
        }
        head[data->group]->next = NULL; //make sure the next is set to NULL
        pthread_create(&head[data->group]->data, NULL, request, data);
    }

    //wait for the priority group to be finished
    struct thread *tmp_head = group_threads[current_group];
    while (tmp_head != NULL) {
        pthread_join(tmp_head->data, NULL);
        tmp_head = tmp_head->next;
    }

    //Change the current executing group and broadcast to all waiting threads
    pthread_mutex_lock(&current_group_lock);
    printf("\nAll users from Group %d finished their execution\n", current_group + 1);
    current_group = current_group == 0 ? 1 : 0;
    printf("The users from Group %d start their execution\n\n", current_group + 1);
    pthread_cond_broadcast(&current_group_cond);
    pthread_mutex_unlock(&current_group_lock);

    //wait for the other threads to be finished
    tmp_head = group_threads[current_group];
    while (tmp_head != NULL) {
        pthread_join(tmp_head->data, NULL);
        tmp_head = tmp_head->next;
    }

    //lock the summery data and print
    pthread_mutex_lock(&stats.access);
    printf("Total Requests:\n");
    for (int i = 0; i < GROUP_COUNT; i++) {
        printf("\tGroup %d: %d\n", i + 1, stats.request_count[i]);
    }
    printf("\n");
    printf("Requests that waited:\n");
    printf("\tDue to its group: %d\n", stats.group_wait_count);
    printf("\tDue to a locked position: %d\n", stats.locked_pos_wait_count);
    pthread_mutex_unlock(&stats.access);
}


void *req(void *_data) { // request 
    struct request_data *data = (struct request_data *)_data;
    sleep(data->req_time);
    printf("User %d from Group %d arrives to the DBMS\n", data->user + 1, data->group + 1);

    //update the summery
    pthread_mutex_lock(&stats.access);
    stats.request_count[data->group]++;
    pthread_mutex_unlock(&stats.access);

    //If my group is not the current executing group, wait until it's changed by the main thread
    pthread_mutex_lock(&current_group_lock);
    while (current_group != data->group) {
        printf("User %d is waiting due to its group\n", data->user + 1);

        //update the summery
        pthread_mutex_lock(&stats.access);
        stats.group_wait_count++;
        pthread_mutex_unlock(&stats.access);

        pthread_cond_wait(&current_group_cond, &current_group_lock);
    }
    pthread_mutex_unlock(&current_group_lock);

    //If the requested position is used by another user, wait until it's released
    pthread_mutex_lock(&database[data->req_pos].access);
    while (database[data->req_pos].used_by >= 0) {
        printf("User %d is waiting: position %d of the database is being used by user %d\n", data->user + 1, data->req_pos + 1, database[data->req_pos].used_by);

        //update the summery
        pthread_mutex_lock(&stats.access);
        stats.locked_pos_wait_count++;
        pthread_mutex_unlock(&stats.access);

        pthread_cond_wait(&database[data->req_pos].data_access, &database[data->req_pos].access);
    }
    //update the used by status
    database[data->req_pos].used_by = data->user;
    pthread_mutex_unlock(&database[data->req_pos].access);

    printf("User %d is accessing the position %d of the database for %d second(s)\n", data->user + 1, data->req_pos + 1, data->req_duration);
    sleep(data->req_duration);

    //revert the used by status to default so that another thread can access the entry, and broadcast to all waiting threads.
    pthread_mutex_lock(&database[data->req_pos].access);
    database[data->req_pos].used_by = -1;
    pthread_cond_broadcast(&database[data->req_pos].data_access);
    pthread_mutex_unlock(&database[data->req_pos].access);

    printf("User %d finished its execution\n", data->user + 1);
    return NULL;
}