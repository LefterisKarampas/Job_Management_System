#ifndef _POOL_LIST_H_
#define _POOL_LIST_H_


struct pool_data{
	int num_jobs;
	int exec_jobs;
    int pid;
    int pool_id;
    char *str;
    int str_counter;
    int mul_len;
    int fd_in;
    int fd_out;
};

struct job_data{
	int job_id;
	int status;
	double start;
	double sum_suspend;
	double start_suspend;
};

typedef union data{
	struct pool_data pool;
	struct job_data job;
}Data;

typedef struct NodeTag {
	Data *data;
    struct NodeTag *Next;
} NodeType;

typedef struct list{
	int count;
	NodeType * head;
}List;


void List_InsertFirstNode(NodeType **,Data *);	
int List_DeleteNode_Pool(NodeType **,int);
void List_Initialize(List *);
void List_Insert(List *,Data *);
void List_Remove_Pool(List *,int);
void List_Clear_Job(NodeType *);
void List_ChangeStatus(NodeType * ,int ,int);

#endif