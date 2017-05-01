#ifndef _COORD_H_
#define _COORD_H_

#include "list.h"

void get_args(int,char **,char **,char **,char **,int *);
void Create_pool(List *,int,char *,char *);
void parse(char *,char *,List *,List *,int *,int,char *,int *);
void read_pipes(List *,List *,int,int);
void Check_Pool(List * ,int );
#endif
