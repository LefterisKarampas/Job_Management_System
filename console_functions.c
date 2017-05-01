#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define LEN 128

extern volatile sig_atomic_t stop;

int get_args(int argc,char *argv[], char **jms_out,char **jms_in,char **op_file){
	int i,x;
	for(i=1;i<argc;i=i+2){						//Read the program's arguments from command line
		if(!strcmp(argv[i],"-w")){				// -w flag for jms_in name_pipe
			x = strlen(argv[i+1]);
			*jms_out = malloc(sizeof(char)*(x+1));
			if(*jms_out == NULL){
				fprintf(stderr,"Error malloc jms_in!\n");
				exit(2);
			}
			strcpy(*jms_out,argv[i+1]);
			(*jms_out)[x] = '\0';
		}
		else if(!strcmp(argv[i],"-r")){			// -r flag for jms_out name_pipe
			x = strlen(argv[i+1]);
			*jms_in = malloc(sizeof(char)*(x+1));
			if(*jms_in == NULL){
				fprintf(stderr,"Error malloc jms_out!\n");
				exit(2);
			}
			strcpy(*jms_in,argv[i+1]);
			(*jms_in)[x] = '\0';
		}
		else if(!strcmp(argv[i],"-o")){			// -o flag for file from which read jobs (optional)
			int x = strlen(argv[i+1]); 
			*op_file = malloc(sizeof(char)*(x+1));
			if(*op_file == NULL){
				fprintf(stderr,"Error malloc operation file!\n");
				exit(2);
			}
			strcpy(*op_file,argv[i+1]);
			(*op_file)[x] = '\0';
			printf("Input file: %s\n",*op_file);
		}
		else{
			fprintf(stderr,"Unknown flag: %s\n",argv[i]);
			exit(2);
		}
	}
	if((*jms_out == NULL) || (*jms_in == NULL)){					//Check for arguments
		fprintf(stderr,"No argument for jms_in - jms_out!\n");
		exit(1);
	}
	return 1;
}

int read_file(FILE *fp,int fd_in,int fd_out){
	char buff[LEN];
	int n = 1;
	size_t len = 100;
	int flag;
	while(1){
		stop = 0;													//Flag from signal handler to inform that jms_coord answer
		flag = 0;
		if((fgets(buff, len,fp) == NULL) || (feof(fp))){			//Get a line
			break;
		}
		write(fd_out,buff,strlen(buff)+1);							//Write to named-pipe in order to reading from jms_coord
		if(!strcmp(buff,"shutdown\n")){								//if read shutdown exit
			return -1;
		}
		while((!stop) || (n > 0) || (!flag)){						
			if(stop){
				flag = 1;
			}
			n = read(fd_in,buff,LEN-1);
			if(n < 0){
				continue;
			}
			else{
				buff[n] = '\0';
				int i = 0;
				while(i < n){										//Modify Message for pretty print 
					if(buff[i] == '\0'){
						buff[i] = '\n';								
					}
					else if(buff[i] == '\n'){
						buff[i] = ' ';
					}
					i++;
				}
			}
			printf("%s",buff);										//Print message
			fflush(stdout);
		}
		printf("\n");
	}
	return 0;
}
