#ifndef _CLIENTINFO_H
#define _CLIENTINFO_H

typedef struct {
  char myfifo[100];//用于用户登陆/查询/退出的管道
  char otherfifo[100];//用户发送聊天信息管道
  char content[100];//用户聊天信息
}CLIENTINFO;

#endif
