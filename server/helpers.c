#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <netinet/tcp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#define MAXLINE 4096
#define BACKLOGS 1024
#define TRUE 1
#define FALSE 0

#define GRN  "\x1B[32m"
#define RED  "\x1B[31m"
#define WHT   "\x1B[37m"
#define BLU  "\x1B[34m"
#define YEL  "\x1B[33m"
#define MAG  "\x1B[35m"
#define CYN  "\x1B[36m"
#define RESET "\x1B[0m"

void rm_lt_spaces(char *str){
    int left = 0, right = strlen(str) - 1;

    while (isspace((unsigned char) str[left]))
        left++;

    while ((right >= left) && isspace((unsigned char) str[right]))
        right--;

	int i;

    for (i = left; i <= right; i++)
        str[i - left] = str[i];

    str[i - left] = '\0';
    
    return;
}

int get_client_ip_port(char *str, char *client_ip, int *client_port){
	printf("Request received\n");
	char *n1, *n2, *n3, *n4, *n5, *n6;
	int x5, x6;

	strtok(str, " ");
	n1 = strtok(NULL, ",");
	n2 = strtok(NULL, ",");
	n3 = strtok(NULL, ",");
	n4 = strtok(NULL, ",");
	n5 = strtok(NULL, ",");
	n6 = strtok(NULL, ",");

	sprintf(client_ip, "%s.%s.%s.%s", n1, n2, n3, n4);

	x5 = atoi(n5);
	x6 = atoi(n6);
	*client_port = (256*x5)+x6;

	printf("Client's IP: %s Port: %d\n", client_ip, *client_port);
	return 1;
}

int setup_data_connection(int *fd, char *client_ip, int client_port, int server_port){
	
	struct sockaddr_in cliaddr, tempaddr;

	if ( (*fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    	perror("err: socket error");
    	return -1;
    }

	//bind port for data connection to be server port - 1 by using a temporary struct sockaddr_in
	memset(&tempaddr, '\0', sizeof(tempaddr));
    tempaddr.sin_family = AF_INET;
    tempaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    tempaddr.sin_port   = htons(server_port-1);

    while((bind(*fd, (struct sockaddr*) &tempaddr, sizeof(tempaddr))) < 0){
    	//perror("bind error");
    	server_port--;
    	tempaddr.sin_port = htons(server_port);
    }


	//initiate data connection fd with client ip and client port             
    memset(&cliaddr, '\0', sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port   = htons(client_port);
    if (inet_pton(AF_INET, client_ip, &cliaddr.sin_addr) <= 0){
    	perror("err: inet_pton error");
    	return -1;
    }

    if (connect(*fd, (struct sockaddr *) &cliaddr, sizeof(cliaddr)) < 0){
    	perror("err: connect error");
    	return -1;
    }

    return 1;
}

int get_filename(char *input, char *fileptr){

    char *filename = NULL;
    filename = strtok(input, " ");
    filename = strtok(NULL, " ");
    if(filename == NULL){
        return -1;
    }else{
    	strncpy(fileptr, filename, strlen(filename));
        return 1;
    }
}

int get_cmd_code(char *command){
	char temp_cmd[1024];
	strcpy(temp_cmd, command);
	char *str = strtok(temp_cmd, " ");
	int cmd_code;

	// command code
    if(strcmp(str, "LIST") == 0) 		cmd_code = 1; 
    else if(strcmp(str, "RETR") == 0) 	cmd_code = 2;
    else if(strcmp(str, "SKIP") == 0) 	cmd_code = 4;
    else if(strcmp(str, "ABOR") == 0)	cmd_code = 5;

    return cmd_code;
}

int _ls(int controlfd, int datafd, char *input){
	char filelist[1024], sendline[MAXLINE+1], str[MAXLINE+1];
	memset(filelist, '\0', (int)sizeof(filelist));

	if(get_filename(input, filelist) > 0){

		sprintf(str, "ls %s", filelist);
		printf("Filelist: %s\n", filelist);
		rm_lt_spaces(filelist);
    	DIR *dir = opendir(filelist);
    	if(!dir){
    		sprintf(sendline, "No such file or directory\n");
    		write(controlfd, sendline, strlen(sendline));
    		return -1;
    	}else{closedir(dir);}

	}else{
		sprintf(str, "ls");
	}

	//  initiate file pointer for popen()
    FILE *in;
    extern FILE *popen();

    if (!(in = popen(str, "r"))) {
    	sprintf(sendline, "Abort action. Local error in processing\n");
    	write(controlfd, sendline, strlen(sendline));
        return -1;
    }

    while (fgets(sendline, MAXLINE, in) != NULL) {
        write(datafd, sendline, strlen(sendline));
        printf("%s", sendline);
        memset(sendline, '\0', (int)sizeof(sendline));
    }

    sprintf(sendline, "status: successful");
    write(controlfd, sendline, strlen(sendline));
    pclose(in);

    return 1;
}

int _get(int controlfd, int datafd, char *input){
	char filename[1024], sendline[MAXLINE+1], str[MAXLINE+1];
	memset(filename, '\0', (int)sizeof(filename));
	memset(sendline, '\0', (int)sizeof(sendline));
	memset(str, '\0', (int)sizeof(str));

	
	if(get_filename(input, filename) > 0){
		sprintf(str, "cat %s", filename);

		if((access(filename, F_OK)) != 0){
			sprintf(sendline, "550 No Such File or Directory\n");
    		write(controlfd, sendline, strlen(sendline));
    		return -1;
		}
	}else{
		printf("Filename Not Detected\n");
		sprintf(sendline, "Abort action. Local error in processing\n");
    	write(controlfd, sendline, strlen(sendline));
		return -1;
	}

	FILE *in;
    extern FILE *popen();

    if (!(in = popen(str, "r"))) {
    	sprintf(sendline, "Abort action. Local error in processing\n");
    	write(controlfd, sendline, strlen(sendline));
        return -1;
    }

    while (fgets(sendline, MAXLINE, in) != NULL) {
        write(datafd, sendline, strlen(sendline));
        memset(sendline, '\0', (int)sizeof(sendline));
    }

    sprintf(sendline, "status: successful");
    write(controlfd, sendline, strlen(sendline));
    pclose(in);
    return 1;
}
