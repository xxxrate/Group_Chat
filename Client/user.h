#ifndef _USERINFO_H
#define _USERINFO_H

typedef struct {
	char name[50];//用户在聊天室的名字 传输用
	char cmd[10];//用户登陆/查询/退出命令指令
}USER, *USERPTR;

#endif
