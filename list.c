#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "list.h"


void List_Initialize(List *ListPtr){				//Initialize List
	ListPtr->count = 0;								//Initialize count to 0
	ListPtr->head = NULL;							//Initialize head to NULL
}

void List_Insert(List *list,Data *data){			
	List_InsertFirstNode(&(list->head),data);		//Insert a new node
	list->count++;									//Increase list's count by 1
}


void List_InsertFirstNode(NodeType **node,Data *data){				//List Insert
	NodeType *N, *temp;
    N=(NodeType *)malloc(sizeof(NodeType));
    if(N == NULL){
    	fprintf(stderr,"Fail malloc in List_InsertFirstNode\n");		//If malloc fails,must exiting!
    	return ;
    }
    N->data = data;
    N->Next=NULL;

    if (*node == NULL){								//If head of list is NULL
    	*node=N;									//Insert new node to head
    }
    else {											//Otherwise head is not NULL
        temp=*node;									
        N->Next = temp;								//Previous head connect as next to the new head
        *node=N;									//Insert new node to head
    }
}

void List_Remove_Pool(List *list,int pid){
	int x;
	if(list->count > 0){
		x = List_DeleteNode_Pool(&(list->head),pid);
		if(x == 1){
			list->count--;
		}
	}
}

int List_DeleteNode_Pool(NodeType **node,int pid){
	NodeType *temp,*previous;
	if((*node)->data->pool.pid == pid){					//If have to remove head
		temp = *node;									//Save node to temp
		*node = (*node)->Next;							//Make node to see the next node
		free(temp->data->pool.str);						//Delete temp node
		free(temp->data);
		free(temp);
	}
	else{
		previous = *node;								//Else
		temp = (*node)->Next;							//Search to find node with that pid
		while((temp) && (temp->data->pool.pid != pid)){
			previous = temp;
			temp = temp->Next;
		}
		if(temp == NULL){								//If not find node with that pid 
			return -1;									//Return 
		}
		previous->Next = temp->Next;					//else
		free(temp->data->pool.str);						//delete the node
		free(temp->data);
		free(temp);
	}
	return 1;
}

void List_Clear_Job(NodeType *node){					//Delete Job_List recursively
	if(node->Next != NULL){
		List_Clear_Job(node->Next);
	}
	free(node->data);
	free(node);
}

void List_ChangeStatus(NodeType *node,int jobid,int status){
	while(node){																//Search for job_node with job_id
		if(node->data->job.job_id == jobid){									//If exists
			if((node->data->job.status == 1) && (status == 2)){					//If job was active and now is suspened,
				node->data->job.start_suspend = clock() / CLOCKS_PER_SEC;		//start the timer
			}
			else if((node->data->job.status == 2) && (status == 1)){			//If job was suspened and now is active
				double temp = clock() / CLOCKS_PER_SEC;								
				node->data->job.sum_suspend += temp - node->data->job.start_suspend;		//Hold the overall suspend time
			}
			node->data->job.status = status;												//Change Status
			break;
		}
		node = node->Next;														//Else does not exists, check the next job
	}
}


