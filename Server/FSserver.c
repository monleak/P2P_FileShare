#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <malloc.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

#define MAX_CONN_NUM 1024
#define INVALID_SOCKET -1
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
int countFile = 0;
int countClient = 0;

char* helpMESS = NULL;
char* formatWelcome = "Welcome to my tracking server (ID Client: %d)\n";
char* errorMess= "Something is wrong! Please contact admin.\n";
char* invalidCmd = "INVALID COMMAND!\n";

pthread_mutex_t* mutex = NULL;

typedef struct _file{
    int id; //cfd client sở hữu
    char name[1024];
    char alias[1024];
    char pass[1024];
} file;
file *files = NULL;

typedef struct _client{
  int cfd;
  SOCKADDR_IN caddr;
} client;
client* clients = NULL;

int SendData(int fd, char* data, int len)
{
    int sent = 0;
    int tmp = 0;
    do
    {
        tmp = send(fd, data + sent, len - sent, 0);
        sent += tmp;
    } while (tmp >= 0 && sent < len);
    return sent;
}
int RecvData(int fd, char* data, int maxlen)
{
    int received = 0;
    int blocksize = 1024;
    int tmp = 0;
    do
    {
        tmp = recv(fd, data + received, blocksize, 0);
        received += tmp;
    } while (tmp >= 0 && received < maxlen && tmp == blocksize);
    return received;
}

void processListFile(int cfd, int page){
    char *listFile = NULL;

    pthread_mutex_lock(mutex);

    //Tính tổng số trang
    int countPage = 0;
    if(countFile%10 == 0){
        countPage = countFile/10;
    } else{
        countPage = countFile/10 + 1;
    }
    if( (page>countPage && countPage > 0 )|| page <= 0){
        listFile = (char*)calloc(1024, 1);
        strcat(listFile,"Page bạn nhập không hợp lệ! Hãy kiểm tra lại.");
        goto SENDDATA;
    }

    if(files!=NULL && countFile > 0){
        char *topbotListFile = "=============================================================================\n";
        listFile = (char *) calloc(1, strlen(topbotListFile)+sizeof(char));
        strcat(listFile,topbotListFile);

        int startFile = 10*(page-1);
        int endFile = 10*page < countFile ? 10*page : countFile;
        for(int i=startFile;i< endFile;i++){
            int oldLen = strlen(listFile) < sizeof(listFile) ? sizeof(listFile) : strlen(listFile);
            char* clientId = (char*)calloc(1024, 1);
            sprintf(clientId,"%d | (Client ID %d)\t\t",i,files[i].id);
            listFile = (char*)realloc(listFile, oldLen + strlen(files[i].alias)+ strlen(clientId)+2);
            strcat(listFile,clientId);
            strcat(listFile,files[i].alias);

            if(files[i].id == cfd){
                oldLen = strlen(listFile) < sizeof(listFile) ? sizeof(listFile) : strlen(listFile);
                listFile = (char*)realloc(listFile, oldLen + strlen(" <-- YOU")+2);
                strcat(listFile," <-- YOU");
            }

            strcat(listFile,"\n");
            free(clientId);clientId=NULL;
        }
        int oldLen = strlen(listFile) < sizeof(listFile) ? sizeof(listFile) : strlen(listFile);

        char* pageS = (char*) calloc(1024,1);
        sprintf(pageS,"Page: %d/%d\n",page,countPage);

        listFile = (char *) realloc(listFile,oldLen + strlen(topbotListFile) + strlen(pageS)+2);
        strcat(listFile,pageS);
        free(pageS);pageS=NULL;
        strcat(listFile,topbotListFile);
    }

    SENDDATA:
    pthread_mutex_unlock(mutex);
    if(listFile != NULL && strlen(listFile)>0){
        SendData(cfd,listFile, strlen(listFile));
        free(listFile);
        listFile=NULL;
    }else{
        SendData(cfd,"Không có file nào để hiển thị!", strlen("Không có file nào để hiển thị!"));
    }
}
void processShareFile(int cfd,char* filename,char* pass){
    char* alias = (char*)calloc(1024, 1);
    if(strrchr(filename,'/') != NULL){
        strcpy(alias,strrchr(filename,'/')+1);
    } else{
        strcpy(alias,filename);
    }
    //TODO: thêm password vào
    pthread_mutex_lock(mutex);
    int isExist = 0;
    int countFileSameName = 0; //Đếm số lượng file có cùng tên nhưng khác đường dẫn
    for(int i=0;i<countFile;i++){
        //Check file đã từng được share chưa
        if(files[i].id == cfd && strcmp(files[i].name,filename)==0){
            isExist = 1;
        }else if(files[i].id == cfd && strcmp(files[i].alias,alias)==0){
            countFileSameName++;
        }
    }
    if(isExist == 0){
        int oldLen = sizeof(file)*countFile;
        files = (file*)realloc(files, oldLen + 1*sizeof(file)+2);
        files[countFile].id = cfd;
        strcpy(files[countFile].name,filename);
        strcpy(files[countFile].pass,pass);
        if(countFileSameName == 0){
            strcpy(files[countFile].alias,alias);
        } else{
            strcpy(files[countFile].alias,alias);

            char* temp = (char*)calloc(1024, 1);
            sprintf(temp,"_(%d)",countFileSameName);

            strcat(files[countFile].alias,temp);
            free(temp);
            temp=NULL;
        }
        countFile++;
        SendData(cfd,"Share file thành công!", strlen("Share file thành công!"));
    } else{
        SendData(cfd,"Bạn đã share file này rồi!", strlen("Bạn đã share file này rồi!"));
    }
    pthread_mutex_unlock(mutex);
    free(alias);
    alias = NULL;
}
void processReqDownload(int cfd){
    //TODO:
}
void processFindFile(int cfd, char* filename){
    char *listFile = NULL;
    pthread_mutex_lock(mutex);
    if(files!=NULL && countFile > 0){
        char *topbotListFile = "=============================================================================\n";
        listFile = (char *) calloc(1, strlen(topbotListFile)+sizeof(char));
        strcat(listFile,topbotListFile);

        for(int i=0;i<countFile;i++){
            if(strstr(files[i].alias,filename)!=NULL){
                int oldLen = strlen(listFile) < sizeof(listFile) ? sizeof(listFile) : strlen(listFile);
                char* clientId = (char*)calloc(1024, 1);
                sprintf(clientId,"%d | (Client ID %d)\t\t",i,files[i].id);
                listFile = (char*)realloc(listFile, oldLen + strlen(files[i].alias)+ strlen(clientId)+2);
                strcat(listFile,clientId);
                strcat(listFile,files[i].alias);

                if(files[i].id == cfd){
                    oldLen = strlen(listFile) < sizeof(listFile) ? sizeof(listFile) : strlen(listFile);
                    listFile = (char*)realloc(listFile, oldLen + strlen(" <-- YOU")+2);
                    strcat(listFile," <-- YOU");
                }

                strcat(listFile,"\n");
                free(clientId);clientId=NULL;
            }
        }
        int oldLen = strlen(listFile) < sizeof(listFile) ? sizeof(listFile) : strlen(listFile);
        listFile = (char *) realloc(listFile,oldLen + strlen(topbotListFile) +2);
        strcat(listFile,topbotListFile);
    }
    SENDDATA:
    pthread_mutex_unlock(mutex);
    if(listFile != NULL && strlen(listFile)>0){
        SendData(cfd,listFile, strlen(listFile));
        free(listFile);
        listFile=NULL;
    }else{
        SendData(cfd,"Không tìm được file nào!", strlen("Không tìm được file nào!"));
    }
}
void* ClientThread(void* arg)
{
    int cfd = *((int*)arg);
    free(arg);
    arg = NULL;

    int lenHelpMESS = helpMESS == NULL ? 0 : strlen(helpMESS);
    char* welcome = (char*)calloc(1, sizeof(char)*1024 + lenHelpMESS);
    sprintf(welcome,formatWelcome,cfd);
    if(helpMESS != NULL){
        strcat(welcome,helpMESS);
    }
    SendData(cfd,welcome, strlen(welcome));
    free(welcome);welcome=NULL;

    while (1){
        char buffer[1024] = { 0 };
        int r = RecvData(cfd,buffer, sizeof(buffer));
        if (r > 0){
            while(buffer[strlen(buffer)-1] == '\n' || buffer[strlen(buffer)-1] == '\r'){
                buffer[strlen(buffer)-1] = 0;
            }
            printf("Command from Client %d: %s\n",cfd,buffer);
            if(strncmp(buffer,"fs",2) != 0){
                SendData(cfd,invalidCmd, strlen(invalidCmd));
                continue;
            }
            if(strncmp(buffer,"fs help",7) == 0){
                if(helpMESS!=NULL){
                    SendData(cfd,helpMESS, strlen(helpMESS));
                } else{
                    SendData(cfd,errorMess, strlen(errorMess));
                }
            } else if(strncmp(buffer,"fs list",7) == 0){
                if(strstr(buffer,"-p")!=NULL){
                    int page = atoi(strstr(buffer,"-p")+3);
                    processListFile(cfd,page);
                } else{
                    processListFile(cfd,1);
                }
            } else if(strncmp(buffer,"fs share",8) == 0){
//                processShareFile(cfd,buffer+9);
                if(strstr(buffer,"-p")!=NULL){
                    char* filename = strtok(buffer+8, " ");
                    char* pass = strstr(buffer,"-p")+3;
                    processShareFile(cfd,filename,pass);
                } else{
                    char* filename = strtok(buffer+8, " ");
                    char* pass = "";
                    processShareFile(cfd,filename,pass);
                }
            } else if(strncmp(buffer,"fs find",7) == 0){
                processFindFile(cfd,buffer+8);
            }
            else{
                SendData(cfd,invalidCmd, strlen(invalidCmd));
            }
        }else{
            printf("A client has disconnected (ID: %d)\n",cfd);
            pthread_mutex_lock(mutex);
            if(files!=NULL){ //Xóa các file share của client vừa disconnect
                for(int i=0;i<countFile;i++){
                    if(files[i].id == cfd){
                        if(countFile == 1){
                            free(files);
                            files=NULL;
                            countFile--;
                            break;
                        } else{
                            memmove(&files[i],&files[i+1],sizeof(file)*(countFile-i-1));
                            countFile--;
                            i--;
                            files = (file*) realloc(files,countFile* sizeof(file)+2);
                        }
                    }
                }
            }
            if(clients!=NULL){ //Xóa thông tin client trong danh sách
                for(int i=0;i<countClient;i++){
                    if(clients[i].cfd == cfd){
                        if(countClient == 1){
                            free(clients);
                            clients=NULL;
                            countClient--;
                            break;
                        } else{
                            memmove(&clients[i],&clients[i+1],sizeof(client)*(countClient-i-1));
                            countClient--;
                            i--;
                            clients = (client *) realloc(clients,countClient* sizeof(client)+2);
                        }
                    }
                }
            }
            pthread_mutex_unlock(mutex);
            break;
        }
    }
    close(cfd);
}

int main() {
    mutex = (pthread_mutex_t*)calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(mutex, NULL);

    FILE* f = fopen("./helpCommand", "rb");
    if(f!=NULL){
        fseek(f, 0, SEEK_END);
        int size = ftell(f);
        fseek(f, 0, SEEK_SET);
        helpMESS = (char*)calloc(size, 1);
        fread(helpMESS, 1, size, f);
        fclose(f);
    } else{
        printf("Thiếu file helpCommand\n");
    }

    int sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    SOCKADDR_IN saddr, caddr;
    int clen = sizeof(caddr);
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(8889);
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(sfd, (SOCKADDR*)&saddr, sizeof(saddr)) == 0)
    {
        listen(sfd, 10);
        printf("Waiting for clients ..\n\n");
        while (0 == 0){
            int cfd = accept(sfd, (SOCKADDR*)&caddr, &clen);
            if (cfd != INVALID_SOCKET)
            {
                printf("New client connected! (ID: %d)\n",cfd);
                int oldSize = clients == NULL ? 0 : sizeof(clients);
                clients = (client*) realloc(clients,oldSize + sizeof(client));
                clients[countClient].cfd = cfd;
                clients[countClient].caddr = caddr;
                countClient++;

                pthread_t tid = 0;
                int* arg = (int*)calloc(1, sizeof(int));
                *arg = cfd;
                pthread_create(&tid, NULL, ClientThread, (void*)arg);
            }
        }
    }else
        printf("PORT not available\n");
    if(files!=NULL){
        free(files);
        files = NULL;
    }

    pthread_mutex_destroy(mutex);
    free(mutex);
    close(sfd);
    return 0;
}
