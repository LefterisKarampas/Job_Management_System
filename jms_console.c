#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "console.h"


volatile sig_atomic_t stop;										//Not multi-threading but safe road 

void catchsig(int signo){
	stop = 1;													//Flag for catching signal
}

int main(int argc,char *argv[]){
	char * op_file = NULL;
	char *jms_in = NULL;
	char *jms_out = NULL;
	int fd_in = -1; 
	int fd_out = -1;
	FILE *fp_file;
	int flag_exit = 0;											//Flag for reading a shutdown order

	if((argc < 4) || ((argc+1) % 2 != 0)){
		fprintf(stderr,"Error: Too few arguments!\n");			//Check for program's arguments
		exit(1);
	}

	static struct sigaction act;
	act.sa_handler=catchsig;
	sigfillset(&(act.sa_mask));
	sigaction(SIGTERM,&act,NULL);								//SIGTERM signal handler

	get_args(argc,argv,&jms_out,&jms_in,&op_file);				//Read the arguments
	if (mkfifo(jms_out, 0666) == -1 ){							//Create the name-pipe
		if (errno!=EEXIST) { 									//If it fails	
			perror("jms_console: mkfifo"); 						//Print message
			exit(6); 											//Exit
		}
	}
	if((fd_out = open(jms_out,O_RDWR)) < 0){					//Open name-pipe
		perror("jms_in open problem"); 		
		exit(3);	
	}
	while((fd_in = open(jms_in,O_RDWR|O_NONBLOCK)) < 0);				//Wait until coord makes his named_pipe

	char pid[10];
	sprintf(pid,"%d",getpid());											
	write(fd_out,pid,strlen(pid)+1);									//Send jms_console's pid to jms_coord in order to 
	stop = 0;															//help on the communication

	while(!stop);														//Wait signal from jms_coord

	if(op_file != NULL){											 
		if((fp_file = fopen(op_file,"r")) == NULL){
			fprintf(stderr,"There is not file with name: %s\n",op_file);
		}
		else{
			fprintf(stdout,"----Orders from file!----\n");
			if(read_file(fp_file,fd_in,fd_out) < 0){					//Read orders from file
				flag_exit = 1;
			}
		}
	}
	if(flag_exit == 0){
		fprintf(stdout,"\n----Orders from prompt!----\n");
		read_file(stdin,fd_in,fd_out);									//Read orders from prompt(stdin)
	}

	int n;
	char buff[128];
	while((n = read(fd_in,buff,128))< 0);								//Wait until reading exiting message from jms_coord
	buff[n] = '\0';
	printf("%s\n",buff);												//Print message and exit

	close(fd_out);														//Close file descriptor
	close(fd_in);														//Close file descriptor
	unlink(jms_out);													//Remove the name-pipe
	unlink(jms_in);														//Remove the name-pipe
	free(jms_out);												
	free(jms_in);
	if(op_file != NULL){
		free(op_file);
		if(fp_file != NULL)
			fclose(fp_file);
	}
	fflush(stdout);
	fflush(stderr);
	exit(0);															//Byeee!
}
