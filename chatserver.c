#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#define QUEUESIZE 5
#define BUFFLEN 4096
#define GUSET "guest"
void freeMemory();
typedef struct node_c
{
    int sd;
    char *message;
    struct node_c *next;
} node_c;
//global var
int welcome_socket = 0;
node_c *q_head = NULL;

int connetToServer(int port)
{
    int welcome_socket = 0;
    struct sockaddr_in serv_addr;
    welcome_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (welcome_socket < 0)
    {
        perror("Socket \n");
        return -1;
    }
    serv_addr.sin_family = PF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(welcome_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Bind \n");
        return -1;
    }
    if (listen(welcome_socket, QUEUESIZE) < -1)
    {
        perror("Listen \n");
        return -1;
    }
    return welcome_socket;
}

void printStatus(int fd,int status){
    //if status is 0 equal to read
    //             1 equal to write
    char *temp;
    if(status == 0){

        temp = "read";
    }
    else
    {
        temp= "write";
    }
    printf("server is reday to %s from socket %d\n",temp,fd);
}

void signalHandler(int ds){
    close(welcome_socket);
    freeMemory();
    exit(EXIT_SUCCESS);
}
void freeMemory(){
    node_c *temp;
    while(q_head->next != NULL){
        temp = q_head;
        q_head = q_head->next;
        free(temp->message);
        free(temp);
    }
    if(q_head != NULL){
        free(q_head->message);
        free(q_head);
    }

}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Port is required for run the program\n");
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    if (port < 0)
    {
        perror("Atoi \n");
        exit(EXIT_FAILURE);
    }
    welcome_socket = connetToServer(port);
    signal(SIGINT,signalHandler);
    
    q_head = NULL;
    node_c *q_tail = NULL;
    int q_size = 0 ;
    fd_set clientfd; //all the clients include the main soockt
    fd_set readfd;   // reday to read
    fd_set writefd;  //reday to write to
    FD_ZERO(&clientfd);
    FD_SET(welcome_socket, &clientfd);
    int rs = 0;
    int new_sockfd = 0;
    char temp_read[BUFFLEN];//for read the mess from client
    memset(temp_read,0,BUFFLEN);
    int len_of_read = 0;
    int counter_of_client = welcome_socket;
    while (1)
    {
        readfd = clientfd;
        //nothing to write
        if (q_size == 0)
        {
            FD_ZERO(&writefd);
        }
        else
        {
            writefd = clientfd;
        }
        rs = select(getdtablesize(), &readfd, &writefd, NULL, NULL);
        if (rs == -1)
        {
            perror("Selcect \n");
            close(welcome_socket);
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(welcome_socket, &readfd))
        {
            new_sockfd = accept(welcome_socket, NULL, NULL);
            counter_of_client++;
            if (new_sockfd > 0)
            {
                FD_SET(new_sockfd, &clientfd);
            }
            else
            {
                perror("Accept \n");
                close(welcome_socket);
                exit(EXIT_FAILURE);
            }
        }
        for (int fd = welcome_socket + 1; fd <= counter_of_client; fd++)
        {

            if (FD_ISSET(fd, &readfd))
            {
                printStatus(fd,0);
                //need to read and enter to the queue.
                memset(temp_read,0,BUFFLEN);
                len_of_read = read(fd, temp_read, BUFFLEN);

                if (len_of_read == 0)
                {
                    //client disconect
                    close(fd);
                    FD_CLR(fd, &clientfd);
                }
                else if (len_of_read > 0)
                {
                    //for each mess add to the queue the fd's need to be written
                    for (int wc = welcome_socket + 1; wc <= counter_of_client; wc++)
                    {
                        //enter the queue
                        if (fd != wc && fcntl(wc, F_GETFD)!= -1)
                        {
                            //not add the fd that read from
                            node_c *newNode = malloc(sizeof(node_c) * 1);//(node_c *)
                            if (newNode == NULL)
                            {
                                perror("malloc\n");
                                freeMemory();
                                exit(EXIT_FAILURE);
                            }
                            
                            newNode->sd = wc;
                            char tempNum[10];
                            memset(tempNum,0,10);
                            sprintf(tempNum,"%d",fd);
                            int len_to_add = strlen(tempNum)+ strlen(GUSET) + 2;
                            newNode->message = malloc((len_to_add + len_of_read + 2) * sizeof(char));
                            memset(newNode->message, 0, (len_to_add + len_of_read + 2 ));
                            strcat(newNode->message, GUSET);
                            strcat(newNode->message, tempNum);
                            strcat(newNode->message, ":");//was ": "  means +2 
                            strcat(newNode->message, temp_read);
                            newNode->next = NULL;

                            if (q_head == NULL)
                            {
                                //list is empty
                                q_head = newNode;
                                q_tail = newNode;
                            }
                            else
                            {
                                q_tail->next = newNode;
                                q_tail = newNode;
                            }
                            q_size++;
                        }
                    }
                }

                else
                {
                    perror("read \n");
                    close(welcome_socket);
                    exit(EXIT_FAILURE);
                }
            }
        }


        int temp_q_size = q_size;
        int len_write = 0;
        int counter_mess = 0;
        for(;counter_mess < temp_q_size; counter_mess++)
        {
            if (q_head != NULL){
                node_c *temp = malloc(sizeof(node_c) * 1);
                temp->sd = q_head->sd;
                temp->next = NULL;


                

                temp->message = malloc((strlen(q_head->message)+1) * sizeof(char));
                memset(temp->message, 0, (strlen(q_head->message)+1));
                strcpy(temp->message , q_head->message);
               

                node_c* freenode = q_head;


                if(FD_ISSET(temp->sd, &writefd)){
                    printStatus(temp->sd,1);
                    len_write = write(temp->sd, temp->message, strlen(temp->message)+1);//was 0
                    if (len_write == -1){
                        perror("write\n");
                        freeMemory();
                        exit(EXIT_FAILURE);
                    }
                    q_head = q_head->next;

                    free(freenode->message);
                    free(freenode);
                    q_size --;
                    free(temp->message);
                    free(temp);
                }
                else
                {
                    //insert to the end of the queue
                    q_tail->next = temp;
                    q_tail = temp;
                    q_head = q_head->next;

                    free(freenode->message);
                    free(freenode);
                }

            }

        }
    }
}
