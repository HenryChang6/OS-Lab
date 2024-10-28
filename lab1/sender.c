#include "sender.h"

double time_taken = 0.0;
struct timespec start, end;
sem_t *sender_sem = NULL;
sem_t *receiver_sem = NULL;
mailbox_t mailbox;
 
void cleanup(int signum) {
    printf("\nCaught signal %d, cleaning up resources...\n", signum);
    
    if (sender_sem) 
        sem_close(sender_sem);
    if (receiver_sem) 
        sem_close(receiver_sem);
    if (mailbox.flag == 2) 
        shmdt(mailbox.storage.shm_addr);


    exit(0); 
}

void send(message_t message, mailbox_t* mailbox_ptr){
    // Send logic for Message Passing Mechanism 
    if (mailbox_ptr->flag == 1) {
        // msgsnd() send message for a message queue
        clock_gettime(CLOCK_MONOTONIC, &start);
        int status = msgsnd(mailbox_ptr->storage.msqid, &message, sizeof(message.mtext), 0);
        clock_gettime(CLOCK_MONOTONIC, &end);
        if (status != 0) {
            perror("msgsnd failed");
            exit(1);
        }
        
    } else if (mailbox_ptr->flag == 2) { // Send logic for Shared Mem Mechanism 
        clock_gettime(CLOCK_MONOTONIC, &start);
        strcpy(mailbox_ptr->storage.shm_addr, message.mtext);
        clock_gettime(CLOCK_MONOTONIC, &end);
    }

    if(strcmp(message.mtext,"exit")!=0) 
        printf("\033[36mSending message:\033[0m %s", message.mtext);
    
    time_taken += ( (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9 );    
}

int main(int argc, char *argv[]){
    signal(SIGINT, cleanup);
   // Parse command line paras
   if (argc != 3) {
        perror("Wrong number of parameters ");
        exit(1);
   }

   message_t message;

   char *file_name;

   mailbox.flag = atoi(argv[1]);
   file_name = argv[2];

   if (mailbox.flag == 1) printf("\033[36mMessage Passing\033[0m\n");
   else printf("\033[36mShared Memory\033[0m\n");

    // Set up message queue or shared memory
    sender_sem = sem_open("/sender_sem", O_CREAT, 0644, 1); // Init_value: 1
    if (sender_sem == SEM_FAILED) {
        perror("sender_sem open failed");
        exit(1);
    }

    receiver_sem = sem_open("/receiver_sem", O_CREAT, 0644, 0); // Init_value: 0
    if (receiver_sem == SEM_FAILED) {
        perror("receiver_sem open failed");
        exit(1);
    }

    key_t key = ftok("receiver.c", 'B'); // 'B' is user-defined
    if (key == -1) {
        perror("fail to key failed");
        exit(1);
    }
    if (mailbox.flag == 1) {
        // Message Passing Mechanism 
        // create a new message queue
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

    // open the file 
    FILE *file = fopen(file_name, "r");
    if (file == NULL) {
        perror("Error opening input file");
        exit(1);
    } 

    // Reading mes in the file and send it
    while (fgets(message.mtext, sizeof(message.mtext), file) != NULL) {
        /*
        If the semaphore's value is greater than 0, it is decreased and the sender continues execution.
        If the semaphore's value is 0, the sender will block (pause) and wait until it receives a signal
        */
        sem_wait(sender_sem); // locks the sender semaphore waiting for receiver to get ready

        message.mtype = 1; 
        send(message, &mailbox);

        sem_post(receiver_sem);  // Signal the receiver semaphore (increase)
    }
    
    printf("\n\033[31mEnd of input file! exit!\033[0m\n");
    printf("Total time taken in sending message: %f\n", time_taken);

    // send exit message to reciever
    sem_wait(sender_sem);
    strcpy(message.mtext, "exit");
    send(message, &mailbox);
    sem_post(receiver_sem);

    // release the resource
    fclose(file);
    sem_close(sender_sem);
    sem_close(receiver_sem);
    if (mailbox.flag == 2) 
        shmdt(mailbox.storage.shm_addr);

    return 0;
}