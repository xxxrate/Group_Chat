#ifndef _SHMDATA_H_HEADER
#define _SHMDATA_H_HEADER

#define FIFO_INFO 50
#define MAX 20

struct shared_use_st
{
  int online[MAX];//标记用户是否在线
  int number;//记录在线人数总和
  char user_name[MAX][FIFO_INFO];//用二维数组保存用户名与其对应的私有管道
};

#endif
