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
#include <pthread.h>
#include <sys/stat.h>

#define INVALID_SOCKET -1
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

int P2Pfd = -1; //socket P2P
int checkPORT = 0;
int countFile = 0;
int countReq = 0;
char* dirDownload = NULL; //Đường dẫn tới thư mục chứa file download


typedef struct _file{
    char name[200]; //Đường dẫn đầy đủ của file
    char pass[50]; //Mật khẩu của file
} file;
file *files = NULL; //Danh sách các file được chia sẻ

typedef struct _reqDownload{
    char code[20]; //mã xác minh
} reqDownload;
reqDownload* reqDownloads;

int SendData(int fd, char* data, unsigned long len)
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

int RecvData(int fd, char* data, unsigned long maxlen)
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

char *randstring(size_t length) {
    static char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.-#'?!";
    char *randomString = NULL;
    srand(time(NULL));
    if (length) {
        randomString = (char *)malloc(sizeof(char) * (length +1));
        if (randomString) {
            for (int n = 0;n < length;n++) {
                int key = rand() % (int)(sizeof(charset) -1);
                randomString[n] = charset[key];
            }

            randomString[length] = '\0';
        }
    }
    return randomString;
}

void processSendFile(char* filename, int addr, int port,char* randCode){
    int sendFile_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKADDR_IN saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = addr;
    int error = connect(sendFile_socket, (SOCKADDR*)&saddr, sizeof(saddr));
    if(error!=-1){
        FILE* f = fopen(filename, "rb");
        if (f != NULL)
        {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            char* data = (char*)calloc(size, 1);
            fread(data, 1, size, f);
            fclose(f);

            char* header = (char*) calloc(1024,1);
            char* alias = (char*)calloc(1024, 1);
            if(strrchr(filename,'/') != NULL){
                strcpy(alias,strrchr(filename,'/')+1);
            } else{
                strcpy(alias,filename);
            }
            sprintf(header, "FILE %s %ld %s\n", alias, size,randCode);
            SendData(sendFile_socket,header, strlen(header));

            RecvData(sendFile_socket,header, sizeof(header));

//            SendData(sendFile_socket,data, strlen(data));
            int sent = 0;
            while (sent < size)
            {
                int tmp = send(sendFile_socket, data + sent, size - sent, 0);
                sent += tmp;
            }

            free(data);data = NULL;
            free(header);header=NULL;
            free(alias);alias=NULL;
        }
    }
}
void processRecvFile(int cfd, char* filename, unsigned long size){
    int countFileSameName = 0;
    char* nameFile = (char*) calloc(1024,1);
    strcpy(nameFile,filename);

    while (access(nameFile, F_OK) == 0) {
        // file tồn tại
        countFileSameName++;
        sprintf(nameFile,"%s_(%d)",filename,countFileSameName);
    }
    char buffer[1024] = { 0 };
    char* data = (char*)calloc(size, 1);
    int post_size = 0;
    int r=0;
    while (post_size < size)
    {
        r = recv(cfd, buffer, sizeof(buffer), 0);
        if (r > 0)
        {
            memcpy(data + post_size, buffer, r);
            post_size += r;
        }else
            break;
    }
    if(post_size == size){
        char* pathFile = (char*) calloc(1024,1);
        if(dirDownload != NULL){
            if(dirDownload[strlen(dirDownload)-1] == '/'){
                sprintf(pathFile,"%s%s",dirDownload,filename);
            }else{
                sprintf(pathFile,"%s/%s",dirDownload,filename);
            }
        } else{
            strcpy(pathFile,filename);
        }
        FILE* f = fopen(pathFile,"wb");
        if(f!=NULL){
            fwrite(data,1,size,f);
            fclose(f);
        }
    }
    free(data);data=NULL;
    free(nameFile);nameFile=NULL;
}

void processShareFile(int cfd, char* rootPath, char* pass){
    struct stat s;
    if( stat(rootPath,&s) == 0 ){
        if( s.st_mode & S_IFDIR ){
            //là thư mục
            struct dirent** output = NULL;
            int n = scandir(rootPath, &output, NULL, NULL);
            if (n > 0)
            {
                for1:
                for (int i = 0;i < n;i++){
                    if (output[i]->d_type == DT_REG){
                        //Tập tin thông thường
                        char* pathOfFile = (char*) calloc(1024,1);
                        if(rootPath[strlen(rootPath)-1] == '/'){
                            sprintf(pathOfFile,"%s%s",rootPath,output[i]->d_name);
                        }else{
                            sprintf(pathOfFile,"%s/%s",rootPath,output[i]->d_name);
                        }
//                        printf("Đây là 1 file %s\n",pathOfFile);
                        for(int i=0;i<countFile;i++){ //check xem đã từng share file này chưa
                            if(strcmp(files[i].name,pathOfFile)==0){
                                printf("Bạn đã share file này rồi: %s\n",pathOfFile);
                                goto ENDIF;
                            }
                        }

                        unsigned long oldSize = sizeof(file)*countFile < sizeof(files) ? sizeof(files) : sizeof(file)*countFile;
                        files = (file*) realloc(files,oldSize + sizeof(file));
                        strcpy(files[countFile].name,pathOfFile);
                        strcpy(files[countFile].pass,pass);
                        countFile++;

                        char* request = (char*) calloc(1024,1);
                        sprintf(request,"fs share %s -p %s",pathOfFile,pass);
                        SendData(cfd,request, strlen(request));
                        RecvData(cfd,request,sizeof(request));
                        free(pathOfFile);
                        free(request);

                        ENDIF:
                    }else if (output[i]->d_type == DT_DIR){
                        if(strncmp(output[i]->d_name,".",1)==0){
                            continue;
                        }
                        //Thư mục
                        char* pathOfDir = (char*) calloc(1024,1);
                        if(rootPath[strlen(rootPath)-1] == '/'){
                            sprintf(pathOfDir,"%s%s",rootPath,output[i]->d_name);
                        }else{
                            sprintf(pathOfDir,"%s/%s",rootPath,output[i]->d_name);
                        }
                        processShareFile(cfd,pathOfDir,pass);
                        free(pathOfDir);
                    }
                    free(output[i]);
                }
            }else{
                //Thư mục trống
            }
            free(output);
        }
        else if( s.st_mode & S_IFREG ){
            //it's a file
//            printf("Đây là 1 file %s\n",rootPath);

            for(int i=0;i<countFile;i++){ //check xem đã từng share file này chưa
                if(strcmp(files[i].name,rootPath)==0){
                    printf("Bạn đã share file này rồi: %s\n",rootPath);
                    goto END;
                }
            }
            unsigned long oldSize = sizeof(file)*countFile < sizeof(files) ? sizeof(files) : sizeof(file)*countFile;
            files = (file*) realloc(files,oldSize + sizeof(file));
            strcpy(files[countFile].name,rootPath);
            strcpy(files[countFile].pass,pass);
            countFile++;

            char* request = (char*) calloc(1024,1);
            sprintf(request,"fs share %s -p %s",rootPath,pass);
            SendData(cfd,request, strlen(request));
            RecvData(cfd,request,sizeof(request));
            free(request);
        }else{
            //something else
            printf("Có lỗi gì đó đã xảy ra: %s\n",rootPath);
        }
    }
    else{
        //error
        printf("Có lỗi xảy ra khi cố chia sẻ file tại đường dẫn này: %s\n",rootPath);
    }
    END:
}
void* FileShareThread(void* arg){
    int cfd = *((int*)arg);
    free(arg);
    arg = NULL;

    char buffer[1024] = { 0 };
    int r = RecvData(cfd,buffer, sizeof(buffer));
    SendData(cfd,".", strlen("."));
    if (r > 0){
        /* Format các gói tin gửi đến
         * Yêu cầu truyền file đến địa chỉ: SENDTO <filename> <pass> <addr> <port> <randCode>
         * Header của file gửi đến: FILE <filename> <size> <randCode>
         */
        if(strncmp(buffer,"SENDTO",6)==0){
            char* filename = (char*) calloc(200,1);
            char* pass = (char*) calloc(50,1);
            char* addr = (char*) calloc(100,1);
            char* port = (char*) calloc(100,1);
            char* randCode = (char*) calloc(25,1);
            strtok(buffer+6+1, " ");
            sprintf(filename,"%s",buffer+6+1);
            strtok(buffer+6+1+ strlen(filename)+1, " ");
            sprintf(pass,"%s",buffer+6+1+ strlen(filename)+1);
            strtok(buffer+6+1+ strlen(filename)+1+ strlen(pass)+1, " ");
            sprintf(addr,"%s",buffer+6+1+ strlen(filename)+1+ strlen(pass)+1);
            strtok(buffer+6+1+ strlen(filename)+1+ strlen(pass)+1+ strlen(addr)+1, " ");
            sprintf(port,"%s",buffer+6+1+ strlen(filename)+1+ strlen(pass)+1+ strlen(addr)+1);
            strtok(buffer+6+1+ strlen(filename)+1+ strlen(pass)+1+ strlen(addr)+1+ strlen(port)+1, " ");
            sprintf(randCode,"%s",buffer+6+1+ strlen(filename)+1+ strlen(pass)+1+ strlen(addr)+1+ strlen(port)+1);
            int check = 0;
            for(int i=0;i<countFile;i++){
                if(strcmp(files[i].name,filename) == 0 && strcmp(files[i].pass,pass) == 0){
                    check = 1;
                    break;
                }
            }
            if(check==1){
                processSendFile(filename,atoi(addr),atoi(port),randCode);
            }
            free(filename);filename=NULL;
            free(pass);pass=NULL;
            free(addr);addr=NULL;
            free(port);port=NULL;
            free(randCode);randCode=NULL;
        } else if(strncmp(buffer,"FILE",4)==0){
            char* filename = (char*) calloc(200,1);
            char* size = (char*) calloc(50,1);
            char* randCode = (char*) calloc(25,1);
            strtok(buffer+4+1, " ");
            sprintf(filename,"%s",buffer+4+1);
            strtok(buffer+4+1+ strlen(filename)+1, " ");
            sprintf(size,"%s",buffer+4+1+ strlen(filename)+1);
            strtok(buffer+4+1+ strlen(filename)+1+ strlen(size)+1, "\n");
            sprintf(randCode,"%s",buffer+4+1+ strlen(filename)+1+ strlen(size)+1);

            int check=0;
            for(int i=0;i<countReq;i++){
                if(strncmp(reqDownloads[i].code,randCode,19)==0){
                    //xóa req trong list
                    check=1;
                    if(countReq == 1){
                        free(reqDownloads);
                        reqDownloads=NULL;
                        countReq--;
                        break;
                    } else{
                        memmove(&reqDownloads[i],&reqDownloads[i+1],sizeof(reqDownload)*(countReq-i-1));
                        countReq--;
                        reqDownloads = (reqDownload *) realloc(reqDownloads,countReq* sizeof(reqDownload)+2);
                    }
                    break;
                }
            }
            if(check == 1){
                processRecvFile(cfd,filename,atoi(size));
            }
            free(filename);filename=NULL;
            free(size);size=NULL;
            free(randCode);randCode=NULL;
        }
    }
    close(cfd);
}
void* P2PThread(void* arg){
    char* port = (char*)arg;

    int p2p_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKADDR_IN p2paddr,caddr;
    int clen = sizeof(caddr);
    p2paddr.sin_family = AF_INET;
    p2paddr.sin_port = htons(atoi(port));
    p2paddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(p2p_socket, (SOCKADDR*)&p2paddr, sizeof(p2paddr)) == 0){
        checkPORT = 1;
        listen(p2p_socket, 10);
        while (0 == 0){
            int cfd = accept(p2p_socket, (SOCKADDR*)&caddr, &clen);
            if (cfd != INVALID_SOCKET)
            {
                pthread_t tid = 0;
                int* arg = (int*)calloc(1, sizeof(int));
                *arg = cfd;
                pthread_create(&tid, NULL, FileShareThread, (void*)arg);
            }
        }
    }else{
        printf("\nPORT not available\n");
        exit(0);
    }
    close(p2p_socket);
}
int main(int argc, char *argv[]){
    if(argc != 2){
        printf("Usage: ./FSClient <port>\n");
        return 1;
    }
    dirDownload = (char*) calloc(1024,1);
    //Tạo 1 luồng để nhận kết nối TCP
    pthread_t tid = 0;
    char* arg = argv[1];
    pthread_create(&tid, NULL, P2PThread, (void*)arg);

    //Đợi kiểm tra cổng kết nối
    while(!checkPORT){
        sleep(1);
    }

    int cfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKADDR_IN saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(8889);
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    int error = connect(cfd, (SOCKADDR*)&saddr, sizeof(saddr));
    if (error != -1)
    {
        SendData(cfd, argv[1], strlen(argv[1]));
        while (0 == 0){
            char buffer[1000] = { 0 };

            //TODO: Fix lỗi khi yêu cầu gửi đến quá dài vượt qua giới hạn của buffer (Tạm thời giới hạn độ dài gửi khi sử dụng fs find)
            RecvData(cfd, buffer, sizeof(buffer));
            printf("%s\n", buffer);

            NHAPLENH:
            printf(">");
            char command[1000] = { 0 };
            fgets(command, sizeof(command), stdin);
            while(command[strlen(command)-1] == '\n' || command[strlen(command)-1] == '\r'){
                command[strlen(command)-1] = 0;
            }
            if (strncmp(command, "fs quit", 7) == 0)
            {
                printf("Disconnected!\n");
                break;
            } else if (strncmp(command, "fs share", 8) == 0)
            {
                char* cloneCommand = (char *) calloc(1024,1);
                strcpy(cloneCommand,command);
                char* filename = NULL;
                char* pass = NULL;
                if(strstr(cloneCommand,"-p")!=NULL){
                    filename = strtok(cloneCommand+8, " ");
                    pass = strtok(cloneCommand+8+1+ strlen(filename)+3, " ");
                } else{
                    filename = strtok(cloneCommand+8, " ");
                    pass = "****";
                }
                processShareFile(cfd,filename,pass);
                free(cloneCommand);cloneCommand=NULL;
                goto NHAPLENH;
            } else if(strncmp(command, "fs test", 7) == 0){
                FILE* f = fopen("/home/monleak/Code/P2P-FileShare/TestCommand/test1","r");
                if(f!=NULL){
                    while(1){
                        if(!fgets(command, sizeof(command),f)){
                            break;
                        }
                        SendData(cfd,command, strlen(command));
                        RecvData(cfd, buffer, sizeof(buffer));
                    }
                    fclose(f);
                    goto NHAPLENH;
                }
            } else if(strncmp(command, "fs downloadLocation", 19) == 0){
                char* downloadLocation = strtok(command+19, " ");

                struct stat s;
                if( stat(downloadLocation,&s) == 0 ){
                    if( s.st_mode & S_IFDIR ){
                        //là thư mục
                        strcpy(dirDownload,downloadLocation);
                        printf("Đặt thành công đường dẫn tới file download: %s\n",dirDownload);
                    }else if( s.st_mode & S_IFREG ){
                        //it's a file
                        printf("Đường dẫn này không trỏ đến 1 thư mục: %s\n",downloadLocation);
                    }else{
                        //something else
                        printf("Có lỗi gì đó đã xảy ra: %s\n",downloadLocation);
                    }
                }else{
                    //error
                    printf("Đường dẫn không hợp lệ: %s\n",downloadLocation);
                }

                goto NHAPLENH;
            } else if(strncmp(command, "fs download", 11) == 0){
                while (command[strlen(command)-1] == '\n'){
                    command[strlen(command)-1] = 0;
                }
                strcat(command," ");
                char* randCode = randstring(19);
                strcat(command,randCode);

                unsigned long oldSize = sizeof(reqDownload)*countReq;
                reqDownloads = (reqDownload *) realloc(reqDownloads,oldSize + sizeof(reqDownload));
                strcpy(reqDownloads[countReq].code,randCode);
                countReq++;

                free(randCode);randCode=NULL;

                SendData(cfd,command, strlen(command));
                char* resDownload = (char*) calloc(1024,1);
                RecvData(cfd, resDownload, sizeof(resDownload));
                printf("%s\n", resDownload);
                if(strncmp(resDownload,"Đã gửi yêu cầu tới máy chưa file!", strlen("Đã gửi yêu cầu tới máy chưa file!")) != 0){
                    //Yêu cầu download bị lỗi (sai mật khẩu, sai ID,...)
                    if(countReq == 1){
                        free(reqDownloads);
                        reqDownloads=NULL;
                        countReq--;
                    } else{
                        countReq--;
                        reqDownloads = (reqDownload*) realloc(reqDownloads,countReq * sizeof(reqDownload)+2);
                    }
                }
                free(resDownload);resDownload=NULL;
                goto NHAPLENH;
            }
            SendData(cfd,command, strlen(command));
        }
        close(cfd);
    } else{
        printf("Không thể kết nối tới P2P Server\n");
    }
    free(dirDownload);
    return 0;
}
