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
#include <time.h>

#define LEN 128


typedef struct job_table{
	int pid;
	int status;
}Job_Table;

Job_Table *Job = NULL;
int max_jobs = 0;
int current_jobs = 0;
int exec_jobs = 0;
int pool_id;

void catchild(int signo){							//Catch SIGCHLD
	int i,status,x;
	char *str;
	int count = 0;
	str = malloc(sizeof(char)*1024);				//Create message for inform jms_coord for finished job
	while((x = waitpid(-1,&status,WNOHANG)) > 0){
		i = 0;
		while((i<current_jobs) && (Job[i].pid != x)){
			i++;
		}
		if(count == 0){
			sprintf(str,"!%d",(pool_id-1)*max_jobs+i+1);
			count++;
		}
		else{
			sprintf(str,"%s %d",str,(pool_id-1)*max_jobs+i+1);
			count++;
		}
		Job[i].status = 0;
		exec_jobs--;
	}
	if(count > 0){
		if((exec_jobs == 0) && (current_jobs == max_jobs)){		//If pools is full from jobs and all are finished
			sprintf(str,"%s %d!",str,-1);						//Send message to finish
		}
		else{
			str[strlen(str)] = '!';								
			str[strlen(str)+1] = '\0';
		}
		write(1,str,strlen(str)+1);									
	}
	free(str);
}

void catchkill(int signo){									//SIGTERM signal handler
	int i;
	if(Job == NULL){
		exit(0);
	}
	int count = 0;
	for(i=0;i<max_jobs;i++){
		if((Job[i].status == 1) || (Job[i].status == 2)){	//If job is active or suspened
			count++;										//Counter for active jobs
			kill(Job[i].pid,SIGKILL);						//send kill signal
			Job[i].status = 0;								//Change status to finished
		}
	}
	free(Job);
	exit(count);
}

int main(int argc,char *argv[]){
	pid_t pid;
	max_jobs = atoi(argv[2]);
	pool_id = atoi(argv[3]);
	Job = malloc(sizeof(Job_Table) *max_jobs);			//Allocate job Table
	int i;
	for(i=0;i<max_jobs;i++){
		Job[i].status = -1;								//Initialize job Table
	}

	static struct sigaction act;
	act.sa_handler=catchkill;
	sigfillset(&(act.sa_mask));
	sigaction(SIGTERM,&act,NULL);						//SIGTERM handler

	static struct sigaction actchild;
	actchild.sa_handler=catchild;
	sigfillset(&(actchild.sa_mask));
	sigaction(SIGCHLD,&actchild,NULL);					//SIGCHLD handler

	char *order;
	int n,x,y;
	char buff[LEN];
	char str[LEN];
	while(1){
			if((n = read(0,buff,LEN-1)) < 0){
				continue;
			}
			buff[n] = '\0';
			order = strtok(buff,"|");
			x = atoi(order);
			if(x==-1){								//EXIT (-1|)
				break;
			}
			else if(x == 6){						//SUSPEND <JOBID> (6|X)
				order = strtok(NULL,"|");
				y = atoi(order);
				if(y > max_jobs){					//Convert jobid to Job_Table index 
					x = y % max_jobs;
					if(x == 0){
						x = max_jobs;
					}
				}
				else{ 
					x = y;
				}
				if(x <= current_jobs){		
					if(Job[x-1].status == 1){									//If job is active
						if(kill(Job[x-1].pid,SIGSTOP) == 0){					//Send stop signal
							Job[x-1].status = 2;											//Change job's status
							sprintf(str,"$ %d $Sent suspend signal to JobID %d?",y,y);		//Message for success
						}
						else{
							sprintf(str,"Fail sent suspend signal to JobID %d?",y);			//Message for failure
						}
					}
					else if(Job[x-1].status == 0){								//If job is finished
						sprintf(str,"JobID %d is already Finished?",y);			//Don't send signal, only send back a message
					}
					else{														//If job is already suspened
						sprintf(str,"JobID %d Status is already Suspened?",y);	//Don't send signal, only send back a message
					}
				}
				else{															//Job is not exists
					sprintf(str,"Job does not exist with JobID: %d?",y);
				}
				write(1,str,strlen(str)+1);
			}
			else if(x == 7){					//RESUME <JOBID> (7|X)>
				order = strtok(NULL,"|");
				y = atoi(order);
				if(y <= max_jobs){ 				//Convert jobid to Job_Table index 
					x = y;
				}
				else{
					x = y % max_jobs;
					if(x == 0){
						x = max_jobs;
					}
				}
				if(x <= current_jobs){
					if(Job[x-1].status == 1){
						sprintf(str,"JobID %d is already Running?",y);
					}
					else if(Job[x-1].status == 0){
						sprintf(str,"JobID %d is already Finished?",y);
					}
					else{
						if(kill(Job[x-1].pid,SIGCONT) == 0){
							Job[x-1].status = 1;
							sprintf(str,"# %d #Sent resume signal to JobID %d?",y,y);
						}
						else{
							sprintf(str,"Fail sent resume signal to JobID %d?",y);
						}
					}
				}
				else{
					sprintf(str,"Job does not exist with JobID: %d?",y);
				}
				write(1,str,strlen(str)+1);
			}
			else if(x == 0){						//SUBMIT <JOB> (0|JOB)
				pid = fork();						//New job creation
				if(pid < 0){
					perror("fork-job");
					exit(0);
				}
				if(pid == 0){
					order = strtok(NULL,"|");
					time_t rawtime;					
					struct tm * timeinfo;		
					time ( &rawtime );
					timeinfo = localtime (&rawtime);		//Take time for job's directory
					char exec[15];							//Store the execution program
					char **arguments;						//Arguments for execution program
					int w = 5;
					arguments = malloc(sizeof(char *)*w);	
					order = strtok(order," ");				//Take the program
					strcpy(exec,order);						//Store it
					arguments[0] = malloc(sizeof(char)*(strlen(order)+1));	//Store it also as first argument 
					strcpy(arguments[0],order);
					int q = 1;
					while((order = strtok(NULL," ")) != NULL){						//Until read all the arguments
						arguments[q] = malloc(sizeof(char)*(strlen(order)+1));
						strcpy(arguments[q],order);
						q++;
						if(q % 5 == 0){
							arguments = realloc(arguments,sizeof(char *)*w*2);
							w = w*2;
						}
					}
					arguments[q] = NULL;											//Finish with the arguments
					int fd_out,fd_err;
					int jobid = (pool_id-1)*max_jobs+current_jobs+1;
					sprintf(str,"%s/sdi1400064_%d_%d_%d%d%d_%d%d%d",argv[1],jobid,getpid(),		//Create directory's name
					timeinfo->tm_year + 1900,timeinfo->tm_mon + 1,timeinfo->tm_mday,
					timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
					struct stat st = {0};
					if (stat(str, &st) == -1) {
						mkdir(str, 0700);														//Make directory
					}
					char *nstr;
					nstr = malloc(sizeof(char)*(strlen(str)+15));
					sprintf(nstr,"%s/stdout_%d",str,jobid);										//Create job's stdout_file
					fd_out = open(nstr,O_RDWR|O_CREAT,0666);
					dup2(fd_out,1);																//stdout -> stdout_file
					sprintf(nstr,"%s/stderr_%d",str,jobid);										//Create job's stderr_file
					fd_err = open(nstr,O_WRONLY|O_CREAT,0666);
					free(nstr);
					dup2(fd_err,2);																//stderr -> stdout_file
					close(fd_out);
					close(fd_err);
					execvp(exec,arguments);														
					perror("Fail execvp");
					exit(0);
				}
				else{
					int jobid = (pool_id-1)*max_jobs+current_jobs+1;
					Job[current_jobs].pid = pid;
					Job[current_jobs].status = 1;
					current_jobs++;
					exec_jobs++;
					sprintf(str,"# %d #JobID: %d, PID: %d?",jobid,jobid,pid);
					write(1,str,strlen(str)+1);
				}
		}
	}
	free(Job);
	exit(0);
}
