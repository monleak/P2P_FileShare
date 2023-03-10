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
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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
FILE* flog = NULL;

typedef struct _file{
    int id; //cfd client sở hữu
    char name[200];
    char alias[100];
    char pass[50];
} file;
file *files = NULL;

typedef struct _client{
  int cfd;
  uint32_t addr;
  int port;
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
    pthread_mutex_lock(mutex);
    int isExist = 0;
    int countFileSameName = 0; //Đếm số lượng file có cùng tên nhưng khác đường dẫn
    for(int i=0;i<countFile;i++){
        char* alias1 = (char*)calloc(1024, 1);
        if(strrchr(files[i].name,'/') != NULL){
            strcpy(alias1,strrchr(files[i].name,'/')+1);
        } else{
            strcpy(alias1,files[i].name);
        }
        //Check file đã từng được share chưa
        if(files[i].id == cfd && strcmp(files[i].name,filename)==0){
            isExist = 1;
        }else if(files[i].id == cfd && strcmp(alias1,alias)==0){
            countFileSameName++;
        }
        free(alias1);
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
void processReqDownload(int cfd, int id,char* pass, char* randCode){
    //Kiểm tra tính hợp lệ của yêu cầu gửi đến.
    if(id > countFile-1){
        SendData(cfd,"ID File yêu cầu download không tồn tại!", strlen("ID File yêu cầu download không tồn tại!"));
        goto END;
    }
    if(files[id].id == cfd){
        SendData(cfd,"File yêu cầu nằm trên máy của bạn!", strlen("File yêu cầu nằm trên máy của bạn!"));
        goto END;
    }
    if(strcmp(files[id].pass,pass) == 0){
        int idclient = files[id].id;
        char* filename = (char*) calloc(1024,1);
        strcpy(filename,files[id].name);
        uint32_t addr=0; //địa chỉ máy chưa file
        int port=0; //port máy chứa file
        uint32_t reqaddr=0; //địa chỉ máy yêu cầu
        int reqport=0; //port máy yêu cầu
        for(int i=0;i<countClient;i++){
            if(clients[i].cfd == idclient){
                addr = clients[i].addr;
                port = clients[i].port;
            } else if(clients[i].cfd == cfd){
                reqaddr = clients[i].addr;
                reqport = clients[i].port;
            }
            if(addr != 0 && reqaddr != 0){
                break;
            }
        }
        char* formatMESS = "SENDTO %s %s %u %d %s";
        char* mess = (char *) calloc(1024,1);
        sprintf(mess,formatMESS,filename,pass,reqaddr,reqport,randCode);

        //Kết nối đến kênh p2p của client
        int cp2p = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        SOCKADDR_IN p2paddr;
        p2paddr.sin_family = AF_INET;
        p2paddr.sin_port = htons(port);
        p2paddr.sin_addr.s_addr = addr;
        int error = connect(cp2p, (SOCKADDR*)&p2paddr, sizeof(p2paddr));
        if (error != -1){
            SendData(cp2p,mess, strlen(mess));
        }
        close(cp2p);
        free(filename);filename=NULL;
        free(mess);mess=NULL;
        SendData(cfd,"Đã gửi yêu cầu tới máy chưa file!", strlen("Đã gửi yêu cầu tới máy chưa file!"));
    } else{
        SendData(cfd,"SAI MẬT KHẨU!!!", strlen("SAI MẬT KHẨU!!!"));
    }
    END:
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
        SendData(cfd,listFile, strlen(listFile) <= 1024 ? strlen(listFile) : 1024);
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

        time_t rawtime;
        struct tm * timeinfo;

        if (r > 0){
            while(buffer[strlen(buffer)-1] == '\n' || buffer[strlen(buffer)-1] == '\r'){
                buffer[strlen(buffer)-1] = 0;
            }
            printf("Command from Client %d: %s\n",cfd,buffer);

            time(&rawtime);
            timeinfo = localtime(&rawtime);
            fprintf(flog, "[%d:%d:%d] Command from Client %d: %s\n",timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,cfd,buffer);
            fflush(flog);

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
                    // fs share <filename> -p <pass>
                    char* filename = strtok(buffer+8, " ");
                    char* pass = strtok(buffer+8+1+ strlen(filename)+3, " ");
                    processShareFile(cfd,filename,pass);
                } else{
                    char* filename = strtok(buffer+8, " ");
                    char* pass = "****";
                    processShareFile(cfd,filename,pass);
                }
            } else if(strncmp(buffer,"fs find",7) == 0){
                processFindFile(cfd,buffer+8);
            } else if(strncmp(buffer,"fs download",11) == 0){
                if(strstr(buffer,"-p")!=NULL){
                    // fs download <id> -p <pass>
                    char* id = strtok(buffer+11, " ");
                    char* pass = strtok(buffer+11+1+ strlen(id)+3, " ");
                    char* randCode = strtok(buffer+11+1+ strlen(id)+3+ strlen(pass)+2, " ");
                    processReqDownload(cfd, atoi(id),pass,randCode);
                } else{
                    char* id = strtok(buffer+11, " ");
                    char* pass = "****";
                    char* randCode = strtok(buffer+11+1+ strlen(id)+1, " ");
                    processReqDownload(cfd, atoi(id),pass,randCode);
                }
            }
            else{
                SendData(cfd,invalidCmd, strlen(invalidCmd));
            }
        }else{
            printf("A client has disconnected (ID: %d)\n",cfd);

            time(&rawtime);
            timeinfo = localtime(&rawtime);
            fprintf(flog, "[%d:%d:%d] A client has disconnected (ID: %d)\n",timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,cfd);
            fflush(flog);

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

    //Tạo thư mục chưa các file log ghi lại hoạt động của server
    DIR* dir = opendir("Log");
    if (dir) {
        /* Directory exists. */
        closedir(dir);
    } else if (ENOENT == errno) {
        int check;
        char* dirname = "Log";

        check = mkdir(dirname,0777);

        // check if directory is created or not
        if (!check)
            printf("Đã tạo thư mục Log\n");
        else {
            printf("Không thể tạo thư mục log\n");
            exit(1);
        }
    } else {
        /* opendir() failed for some other reason. */
        printf("opendir() failed for some other reason.\n");
    }
    //Tạo file log ghi lại hoạt động.
    char filePath[30];
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    sprintf(filePath, "Log/%d-%d-%d.log", timeinfo->tm_mday,
            timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);
    flog = fopen(filePath, "a"); // a+ (create + append) option will allow appending which is useful in a log file
    if (flog == NULL) {
        printf("Không mở được file log\n");
        exit(1);
    }else{
        printf("Mở thành công file log %s\n",filePath);
    }
    if (bind(sfd, (SOCKADDR*)&saddr, sizeof(saddr)) == 0)
    {
        listen(sfd, 10);

        printf("Waiting for clients ..\n\n");
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        fprintf(flog, "[%d:%d:%d] Waiting for clients ..\n",timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        fflush(flog);
        while (0 == 0){
            int cfd = accept(sfd, (SOCKADDR*)&caddr, &clen);
            if (cfd != INVALID_SOCKET)
            {
                printf("New client connected! (ID: %d)\n",cfd);
                time(&rawtime);
                timeinfo = localtime(&rawtime);
                fprintf(flog, "[%d:%d:%d] New client connected! (ID: %d)\n",timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,cfd);
                fflush(flog);
                char* port = (char *) calloc(1024,1);
                RecvData(cfd,port, sizeof(port));

                int oldSize = clients == NULL ? 0 : sizeof(clients);
                clients = (client*) realloc(clients,oldSize + sizeof(client));
                clients[countClient].cfd = cfd;
                clients[countClient].addr = caddr.sin_addr.s_addr;
                clients[countClient].port = atoi(port);
                countClient++;
                free(port);port=NULL;

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
    fclose(flog);
    close(sfd);
    return 0;
}
