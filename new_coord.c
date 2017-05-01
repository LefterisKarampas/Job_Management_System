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
#include "list.h"
#include "coord.h"

#define READ 0
#define WRITE 1
#define LEN 128

void get_args(int argc,char **argv,char **jms_out,char **jms_in,char **path,int *num_jobs){
	int i;
	int x;
	for(i=1;i<argc;i=i+2){						//Read the program's arguments from command line
		if(!strcmp(argv[i],"-r")){				// -r flag for jms_in name_pipe
			x = strlen(argv[i+1]);
			*jms_in = malloc(sizeof(char)*(x+1));
			if(*jms_in == NULL){
				perror("malloc");
				exit(2);
			}
			strcpy(*jms_in,argv[i+1]);
			(*jms_in)[x] = '\0';
		}
		else if(!strcmp(argv[i],"-w")){			// -w flag for jms_out name_pipe
			x = strlen(argv[i+1]);
			*jms_out = malloc(sizeof(char)*(x+1));
			if(*jms_out == NULL){
				perror("malloc");
				exit(2);
			}
			strcpy(*jms_out,argv[i+1]);
			(*jms_out)[x] = '\0';
		}
		else if(!strcmp(argv[i],"-l")){			// -l flag for path to directory
			int x = strlen(argv[i+1]); 
			*path = malloc(sizeof(char)*(x+1));
			if(*path == NULL){
				perror("malloc");
				exit(2);
			}
			strcpy(*path,argv[i+1]);			
			(*path)[x] = '\0';
		}
		else if(!strcmp(argv[i],"-n")){			//-n flag for number of jobs per pool
			*num_jobs = atoi(argv[i+1]);
		}
		else{
			fprintf(stderr,"Unknown flag: %s\n",argv[i]);
			exit(2);
		}
	}
	if((*path == NULL) || (*jms_out == NULL) || (*jms_in == NULL) || (*num_jobs == -1)){
		fprintf(stderr,"No argument for jms_in - jms_out - path - num_jobs!\n");
		exit(1);
	}
}


void Create_pool(List *list,int count,char *max_jobs,char *path){
	int in[2];
	int out[2];
	char num[10];
	pid_t pid;
	int x,max;
	max = atoi(max_jobs);
	if(count <= max){
			x = 1;
	}
	else{
		x = count / max;
		if(count % max != 0){
			x += 1;
		}
	}
	if (pipe(in) == -1){								//Create a pipe for jms_coord->pool communication
		perror("pipe");
		exit(3);
	}
	if (pipe(out) == -1){								//Create a pipe for pool->jms_coord communication
		perror("pipe");
		exit(3);
	}
	if((pid = fork())< 0){
		perror("fork");
		exit(2);
	}
	if(pid == 0){
		int retval;
		if((retval = fcntl(in[READ], F_SETFL, fcntl(in[READ], F_GETFL) | O_NONBLOCK)) < 0){			//Non-blocking pipe
			perror("fcntl");
			exit(4);
		}
		dup2(in[READ],0);																			//stdin->in_pipe
		dup2(out[WRITE],1);																			//stdout->out_pipe
		close(in[READ]);		
		close(in[WRITE]);
		close(out[READ]);
		close(out[WRITE]);
		sprintf(num,"%d",x);
		execlp("./pool","./pool",path,max_jobs,num,NULL);					//Its time to born a new pool
		perror("FAIL execlp");
		exit(1);
	}
	else{
		int retval;
		if((retval = fcntl(out[READ], F_SETFL, fcntl(out[READ], F_GETFL) | O_NONBLOCK)) < 0){	//Non-blocking pipe
			perror("fcntl");
			exit(4);
		}
		
		Data *data;
		data = malloc(sizeof(Data));										//Allocate space for store pool's information
		data->pool.pool_id = x;	
		data->pool.pid = pid;
		data->pool.fd_in = out[READ];										//Read from there
		data->pool.fd_out = in[WRITE];										//Write to there
		data->pool.str_counter = 0;											//Buffer counter
		data->pool.str = malloc(sizeof(char)*LEN);							//Buffer
		data->pool.mul_len = 1;												//Flag for buffer's length
		data->pool.exec_jobs = 0;											//Active jobs
		data->pool.num_jobs = 0;											//Submited jobs
		List_Insert(list,data);												//Insert to Pool_list the new pool
		close(out[WRITE]);
		close(in[READ]);
	}
}

void parse(char *buff,char *str,List *list,List *Jobs,int *count,int max_jobs,char *path,int *pool_id){
	char temp[LEN];
	char order[LEN];
	char *lexem;
	lexem = strtok(str," ");
	int flag=-1;
	//SUBMIT ORDER
	if(!strcmp(lexem,"submit")){														
		++(*count);																		//Jobs' counter
		if((list->head == NULL) || (list->head->data->pool.num_jobs == max_jobs)){		//If pool not exists or is full
			char p[4];
			sprintf(p,"%d",max_jobs);
			Create_pool(list,*count,p,path);											//Create a new pool
		}
		strcpy(temp,&(buff[strlen("submit ")]));										//Take the order part only
		flag = 0;
		sprintf(order,"%d|%s",flag,temp);												//Encode the message for pool
		write(list->head->data->pool.fd_out,order,strlen(order)+1);						//Send it
		list->head->data->pool.num_jobs++;												//Update the pool's information
		list->head->data->pool.exec_jobs++;
		Data *data;
		data = malloc(sizeof(Data));												//Create space to store job's information
		data->job.job_id = (*count);												//Initialize job's information
		data->job.start = clock() / CLOCKS_PER_SEC;
		data->job.status = 1;
		data->job.sum_suspend = 0.0;
		List_Insert(Jobs,data);															//Insert to Job's list
		(*pool_id) = list->head->data->pool.pool_id;							//Flag to know from who i am waiting response
	}
	//STATUS ORDER
	else if(!strcmp(lexem,"status")){											
		strcpy(temp,&(buff[strlen("status ")]));										//Take the jobid									
		int y;
		y = atoi(temp);
		*pool_id = -1;																	//I am not waiting response from a pool
		if(y > (*count)){
			sprintf(temp,"Fail there is not job with job_id: %d",y);					//There is not job with this jobid
			write(1,temp,strlen(temp)+1);
		}
		else{
			NodeType *temp_node;	
			temp_node = Jobs->head;
			while(temp_node){															//Search to job list to find this job
				if(temp_node->data->job.job_id == y){
					if(temp_node->data->job.status == 1){								//Active job message
						double now = clock() /CLOCKS_PER_SEC;
						sprintf(temp,"JobID %d Active (running for %.0lf seconds)\n",temp_node->data->job.job_id,
							now-temp_node->data->job.start -temp_node->data->job.sum_suspend);
					}
					else if(temp_node->data->job.status == 0){							//Finished job message
						sprintf(temp,"JobID %d Finished\n",temp_node->data->job.job_id);
					}
					else{																//Suspened job message
						sprintf(temp,"JobID %d Suspened\n",temp_node->data->job.job_id);
					}
					write(1,temp,strlen(temp)+1);										//Write message
					break;
				}
				temp_node = temp_node->Next;											//If didn't find, check Next
			}
		}
	}
	//STATUS-ALL [n]
	else if(!strcmp(lexem,"status-all")){
		strcpy(temp,&(buff[strlen("status-all ")]));
		flag = 2;
		int y;	
		if(temp[0] != '\0'){															//If we have time-duration argument
			flag = 1;																	//Turn flag = 1
			y = atoi(temp);
		}
		NodeType *temp_node;
		*pool_id = -1;																	//Don't wait response from someone
		temp_node = Jobs->head;
		int q = 1;
		while(temp_node){																//For each job
			double now = clock() /CLOCKS_PER_SEC;
			if(((flag == 1) && (temp_node->data->job.start <= (now-(double)y))) || (flag == 2)){	
				if(temp_node->data->job.status == 1){									//Active job
					sprintf(temp,"%d.  JobID %d Status:  Active (running for %.0lf seconds)\n",q++,temp_node->data->job.job_id,
						now-temp_node->data->job.start - temp_node->data->job.sum_suspend);
				}
				else if(temp_node->data->job.status == 0){								//Finished job
					sprintf(temp,"%d.  JobID %d Status:  Finished\n",q++,temp_node->data->job.job_id);
				}
				else{																	//Suspended job
					sprintf(temp,"%d.  JobID %d Status:  Suspened\n",q++,temp_node->data->job.job_id);
				}
				write(1,temp,strlen(temp)+1);									//Write message to jms_console's reading pipe
			}
			temp_node = temp_node->Next;												//Go to next job
		}
		if(q == 1){
			write(1,"There are not jobs!\n",strlen("There are not jobs\n")+1);
		}
	}
	//SHOW-ACTIVE
	else if(!strcmp(lexem,"show-active")){
		strcpy(temp,&(buff[strlen("show-active ")]));
		*pool_id = -1;																	//Don't wait response from someone
		NodeType *temp_node;
		temp_node = Jobs->head;
		strcpy(temp,"Active Jobs:\n");
		write(1,temp,strlen(temp)+1);
		int q = 1;
		while(temp_node){																//For each job
			double now = clock() /CLOCKS_PER_SEC;
			if(temp_node->data->job.status == 1){									//If job is Active, send message to pipe
				sprintf(temp,"%d.  JobID %d Status:  Active (running for %.0lf seconds)\n",q++,temp_node->data->job.job_id,
					now-temp_node->data->job.start);
				write(1,temp,strlen(temp)+1);											//Send message 
			}
			temp_node = temp_node->Next;
		}
		if(q == 1){																//If there are not active jobs, send message
			write(1,"There are not Active jobs\n",strlen("There are not Active jobs\n")+1);
		}
	}
	//SHOW-POOLS
	else if(!strcmp(lexem,"show-pools")){
		strcpy(temp,&(buff[strlen("show-pools ")]));
		NodeType *temp_node;
		temp_node = list->head;
		strcpy(temp,"Pool & NumOfJobs:\n");
		write(1,temp,strlen(temp)+1);
		int q = 1;
		while(temp_node){				//Print all pools and the number of active(or suspened) jobs for each pool
			sprintf(temp,"%d.  %d %d\n",q++,temp_node->data->pool.pid,temp_node->data->pool.exec_jobs);
			write(1,temp,strlen(temp)+1);
			temp_node = temp_node->Next;
		}
		*pool_id = -1;								//Don't wait response from someone
		if(q == 1){
			write(1,"There are not active pools\n",strlen("There are not active pools\n")+1);
		}
	}
	//SHOW-FINISHED
	else if(!strcmp(lexem,"show-finished")){
		strcpy(temp,&(buff[strlen("show-finished ")]));
		NodeType *temp_node;
		*pool_id = -1;								//Don't wait response from someone
		temp_node = Jobs->head;
		strcpy(temp,"Finished Jobs:\n");
		write(1,temp,strlen(temp)+1);
		int q = 1;
		while(temp_node){							//For each job
			if(temp_node->data->job.status == 0){	//which is finished
				sprintf(temp,"%d.  JobID %d Status:  Finished\n",q++,temp_node->data->job.job_id);
				write(1,temp,strlen(temp)+1);		//Send message for that job
			}
			temp_node = temp_node->Next;			//Check next job
		}
		if(q == 1){
			write(1,"There are not Finished jobs\n",strlen("There are not Finished jobs\n")+1);
		}
	}
	//SUSPEND - RESUME
	else if((!(flag = strcmp(lexem,"suspend"))) || (!strcmp(lexem,"resume"))){
		if(flag == 0){
			strcpy(temp,&(buff[strlen("suspend ")]));
			flag = 6;
		}
		else{
			strcpy(temp,&(buff[strlen("resume ")]));
			flag = 7;
		}
		sprintf(order,"%d|%s",flag,temp);											//Encode message for pool
		int x,y;
		y = atoi(temp);																//Get jobid
		if(y > *count){																//If job does not exists
			sprintf(str,"There is not exists job with JobID: %d",y);				//Send message for that
			write(1,str,strlen(str)+1);
			*pool_id = -1;															//Don't wait response from someone
		}
		else{																		//Else job exists
			x = y / max_jobs;														//Find pool_id which has that job
			if(y % max_jobs != 0)
				x +=1;
			NodeType *temp_node;
			temp_node = list->head;
			while(temp_node){														//Find the pool
				if(temp_node->data->pool.pool_id == x){
					break;
				}
				else{
					temp_node = temp_node->Next;
				}
			}
			if(!temp_node){															//If pool does not exist
				sprintf(str,"There is not exists job with JobID: %d",y);			//Send message to console
				write(1,str,strlen(str)+1);
				*pool_id = -1;
			}
			else{																	//Else
				write(temp_node->data->pool.fd_out,order,strlen(order)+1);			//Send order to pool
				*pool_id = temp_node->data->pool.pool_id;							//Flag to wait response from there
			}
		}
	}
	else{
		strcpy(temp,"Unknown order");
		flag = -1;
		*pool_id = -1;
		write(1,"Unknown order",strlen("Unknown order")+1);
	}		
}


void read_pipes(List *list,List *jobs,int pool_id,int max_jobs){
	NodeType *temp_node;
	char temp[LEN];
	int flag = 0;
	int p;
	int n;
	n = 0;
	if(list->count > 0){
		while(flag != pool_id){					//Until take response,check pipes for infos
			if(pool_id == -1){					//If flag == -1, don't wait response, only check pipes for infos
				flag = pool_id;
			}
			temp_node = list->head;
			while(temp_node){																//For each pool
				p = temp_node->data->pool.str_counter;										//Initialize buffer_count
				while((n = read(temp_node->data->pool.fd_in,temp,LEN)) > 0){				//Read from pool's pipe
					temp[n] = '\0';
					int k = 0;
					while(k < n){															//Parse the message
						if(temp[k] == '?'){													// ? is end of message
							temp[k] = '\n';											
							flag = pool_id;													//Turn on response flag
						}
						else if(temp[k] == '\0'){
							temp[k] = ' ';
						}
						else if(temp[k] == '!'){											// ! job_id ! //
							k++;															//Inform that job with job_id is finished
							int jobid;
							while(temp[k] != '!'){											//In finished message
								if(temp[k] != ' '){
									if((temp[k] == '-') && (temp[k+1] == '1')){							//! -1 !//
										write(temp_node->data->pool.fd_out,"-1|",strlen("-1|")+1);		//Pool ask for exit
										k +=1;
									}
									else{	
										jobid = temp[k] - '0';											//Take job_id
										List_ChangeStatus(jobs->head,jobid,0);							//Update its state
										temp_node->data->pool.exec_jobs--;								
									}
								}
								k++;
							}
							k++;
							continue;
						}
						else if(temp[k] == '#'){										// # job_id # //
							k++;														//Inform that job with job_id is active
							int jobid;
							while(temp[k] != '#'){										//In active message
								if(temp[k] != ' '){
									jobid = temp[k] - '0';								//Take job_id
									List_ChangeStatus(jobs->head,jobid,1);				//Update its state
								}
								k++;
							}
							k++;
							continue;
						}
						else if(temp[k] == '$'){										// $ job_id $ //
							k++;														//Inform that job with job_id is suspened
							int jobid;
							while(temp[k] != '$'){										//In suspened message
								if(temp[k] != ' '){
									jobid = temp[k] - '0';								//Take job_id
									List_ChangeStatus(jobs->head,jobid,2);				//Update its state 
								}
								k++;
							}
							k++;
							continue;
						}
						if(p == temp_node->data->pool.mul_len * LEN){				//Increase the length of buffer before overflow
							temp_node->data->pool.mul_len++;
							temp_node->data->pool.str = realloc(temp_node->data->pool.str,sizeof(char)*(temp_node->data->pool.mul_len * LEN));
						}
						temp_node->data->pool.str[p] = temp[k];						//Copy char from message to buffer
						k++;														
						p++;
					}	
				}
				if((flag == pool_id) && (temp_node->data->pool.pool_id == pool_id)){		//Send message to console when pool responses
					temp_node->data->pool.str[p] = '\0';
					write(1,temp_node->data->pool.str,p+1);
					p = 0;
				}
				temp_node->data->pool.str_counter = p;										//Save the state of buffer(counter)
				temp_node = temp_node->Next;												//Check the next pool
			}
		}
		Check_Pool(list,max_jobs);									//Check for finished pools or realloc too big buffers
	}
}

void Check_Pool(List * Pools,int max_jobs){
	NodeType *current,*previous;
	while((Pools->head) && (Pools->head->data->pool.exec_jobs == 0) && (Pools->head->data->pool.num_jobs == max_jobs)){
		current = Pools->head;									//If head of pool_list is finished remove it
		Pools->head = current->Next;
		free(current->data->pool.str);
		Pools->count--;
		free(current->data);
		free(current);
	}
	if(Pools->head){
		previous = Pools->head;
		if(previous->data->pool.mul_len > 1){
			previous->data->pool.mul_len = 1;
			previous->data->pool.str = realloc(previous->data->pool.str,sizeof(char)*(previous->data->pool.mul_len * LEN));
		}
		previous->data->pool.str_counter = 0;
		current = previous->Next;
		while(current){							//Check all the pools if finished or have to realloc their buffers
			if((current->data->pool.exec_jobs == 0) && (current->data->pool.num_jobs == max_jobs)){		//Finished
				previous->Next = current->Next;
				free(current->data->pool.str);
				Pools->count--;
				free(current->data);
				free(current);
				current = previous->Next;
				continue;
			}
			else if(current->data->pool.mul_len > 1){
				current->data->pool.mul_len = 1;
				current->data->pool.str = realloc(current->data->pool.str,sizeof(char)*LEN);
			}
			current->data->pool.str_counter = 0;
			previous = current;
			current = current->Next;
		}
	}
}
