#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "list.h"
#include "coord.h"

#define READ 0
#define WRITE 1
#define LEN 128


int main(int argc,char *argv[]){
	int fd_in,fd_out;
	int shutdown,num_jobs;
	char *jms_in = NULL;
	char *jms_out = NULL;
	char *path = NULL;
	if(((argc+1) % 2 != 0) || argc < 8){
		fprintf(stderr,"Too few arguments!\n");
		exit(1);
	}
	get_args(argc,argv,&jms_out,&jms_in,&path,&num_jobs);			//Get arguments 
	if (mkfifo(jms_out, 0666) == -1 ){								//Create named-pipe
		if (errno!=EEXIST) { 
			perror("jms_coord: mkfifo"); 
			exit(6); 
		}
	}
	if((fd_out = open(jms_out,O_RDWR)) < 0){						//Open name-pipe to communicate with console
		perror("jms_out open problem");
		exit(3);
	}
	while((fd_in = open(jms_in,O_RDWR|O_NONBLOCK)) < 0);			//Wait until jms_console create the other named-pipe
	struct stat st = {0};
	if (stat(path, &st) == -1) {									//Create path where jobs create their folders
		mkdir(path, 0700);
	}
	char console_pid[10];
	while(read(fd_in,console_pid,10)<0);							//Wait until jms_console, send its pid
	pid_t con_pid = atoi(console_pid);								//Get the pid
	kill(con_pid,SIGTERM);							//Send signal to jms_console that its time to start the communication
	dup2(fd_in,0);									//stdin -> fd_in 
	dup2(fd_out,1);									//stdout -> fd_out

	List *pools;
	List *Jobs;
	pools = malloc(sizeof(List));					//Allocate and initialize pools_list
	List_Initialize(pools);
	Jobs = malloc(sizeof(List));					//Allocate and initialize jobs_list
	List_Initialize(Jobs);

	
	char buff[100];
	char *str;
	int count = 0;
	int m = 0;
	while(1){
		read_pipes(pools,Jobs,-1,num_jobs);			//Check the pipes for information
		if((m = read(fd_in,buff,LEN-1)) > 0){		//Read from jms_console's writing pipe
			str = strtok(buff,"\n");				
			if(!strcmp(str,"shutdown")){			//If take shutdown message
				shutdown = 0;			
				while(pools->count > 0){			//Send SIGTERM message to pools
					kill(pools->head->data->pool.pid,SIGTERM);
					int status;
					while(waitpid(pools->head->data->pool.pid,&status,WNOHANG) <= 0){	//Take the exit code 
						kill(pools->head->data->pool.pid,SIGTERM);
					}
					shutdown += (status >> 8);								//Sum all the active jobs which terminated by the signal
					List_Remove_Pool(pools,pools->head->data->pool.pid);	//Remove pool from pools_list
				}
				break;														//Get out from loop
			}
			else{															//Else not shutdown message
				int pool_id;
				parse(buff,str,pools,Jobs,&count,num_jobs,path,&pool_id);	//Parse the message
				read_pipes(pools,Jobs,pool_id,num_jobs);					//Check pipes for infos-response
				kill(con_pid,SIGTERM);										//Send signal to jms_console for next order
			}
		}
	}
	if(Jobs->count > 0){													//Delete jobs_list
		List_Clear_Job(Jobs->head);
	}
	sprintf(str,"Served %d jobs, %d were still in progress",count,shutdown);	//Send report to jms_console
	write(1,str,strlen(str)+1);
	free(pools);																//Delete pools_list
	free(Jobs);																	//Delete jobs_list
	free(jms_out);																
	free(jms_in);
	free(path);
	fflush(stdout);
	fflush(stderr);
	exit(0);
}