#include "receiver.h"

double time_taken = 0.0;
struct timespec start, end;
sem_t *sender_sem = NULL;
sem_t *receiver_sem = NULL;
mailbox_t mailbox;

void cleanup(int signum) {
    printf("\nCaught signal %d, cleaning up resources...\n", signum);
    
    if (sender_sem) {
        sem_close(sender_sem);
        sem_unlink("/sender_sem");
    }
    if (receiver_sem) {
        sem_close(receiver_sem);
        sem_unlink("/receiver_sem");
    }
    if (mailbox.flag == 2) {
        // Detach from shared memory
        shmdt(mailbox.storage.shm_addr);
    } else if (mailbox.flag == 1) {
        // Remove the message queue
        msgctl(mailbox.storage.msqid, IPC_RMID, NULL);
    }

    exit(0); 
}

void receive(message_t* message_ptr, mailbox_t* mailbox_ptr){
    if (mailbox_ptr->flag == 1) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        int status = msgrcv(mailbox_ptr->storage.msqid, message_ptr, sizeof(message_ptr->mtext), 0, 0);
        clock_gettime(CLOCK_MONOTONIC, &end);
        if (status == -1) {
            perror("msgrcv failed");
            exit(1);
        }
    } else if (mailbox_ptr->flag == 2) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        strcpy(message_ptr->mtext, mailbox_ptr->storage.shm_addr);
        clock_gettime(CLOCK_MONOTONIC, &end);
    }
    if (strcmp(message_ptr->mtext,"exit")!=0) 
        printf("\033[36mReiceiving message:\033[0m %s", message_ptr->mtext);
    
    time_taken += ( (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9 );
    
}

int main(int argc, char *argv[]){
    signal(SIGINT, cleanup);

    if (argc != 2) {
        perror("Wrong number of parameters ");
        exit(1);
    }

    message_t message;

    mailbox.flag = atoi(argv[1]);

    if (mailbox.flag == 1) printf("\033[36mMessage Passing\033[0m\n");
    else printf("\033[36mShared Memory\033[0m\n");

    // Open sem
    sender_sem = sem_open("/sender_sem", O_CREAT, 0644, 1);
    receiver_sem = sem_open("/receiver_sem", O_CREAT, 0644, 0);

    if (sender_sem == SEM_FAILED || receiver_sem == SEM_FAILED) {
        perror("sem_open failed");
        exit(1);
    }

    // Set up message queue or shared memory
    key_t key = ftok("receiver.c", 'B'); // 'B' is user-defined
    if (key == -1) {
        perror("fail to key failed");
        exit(1);
    }
    if (mailbox.flag == 1) {
        int id = msgget(key, 0666 | IPC_CREAT);
        if (id == -1) {
            perror("mssget failed");
            exit(1);
        }
        mailbox.storage.msqid = id;
    } else if (mailbox.flag == 2) {
        // Create or access shared memory (1024 bytes as an example size)
        int shm_id = shmget(key, sizeof(message.mtext), 0666 | IPC_CREAT);
        if (shm_id == -1) {
            perror("shmget failed");
            exit(1);
        }

        // Attach shared memory to the sender process's address space
        char *shm_addr = shmat(shm_id, NULL, 0);
        if (shm_addr == (char *) -1) {
            perror("shmat failed");
            exit(1);
        }

        mailbox.storage.shm_addr = shm_addr;
    }

    // Start Receiving
    while (1) {
        sem_wait(receiver_sem); // wait for the receiver_sem to be unlock

        receive(&message, &mailbox);
        if (strcmp(message.mtext, "exit") == 0) break;

        sem_post(sender_sem); // Unlock the sender semaphore
    }

    printf("\n\033[31mSender Exit!\033[0m\n");
    printf("Total time taken in receiving messages: %f seconds\n", time_taken);

    // Clean up resources
    sem_close(sender_sem);
    sem_close(receiver_sem);
    sem_unlink("/sender_sem");
    sem_unlink("/receiver_sem");

    if (mailbox.flag == 2) {
        // Detach from shared memory
        shmdt(mailbox.storage.shm_addr);
        shmctl(shmget(key, sizeof(message.mtext), 0666 | IPC_CREAT), IPC_RMID, NULL);  // Optionally remove the shared memory
    } else if (mailbox.flag == 1) {
        // Remove the message queue
        msgctl(mailbox.storage.msqid, IPC_RMID, NULL);
    }

    return 0;
}