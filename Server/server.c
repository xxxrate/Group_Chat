#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <signal.h>
#include "shmdata.h"//引入共享内存信息
#include "clientinfo.h"//引入客户端信息
#include "user.h"//引入聊天室用户信息

#define FIFO_CMD "/tmp/chenxiaoguang_command"//第一条管道
#define FIFO_MSG "/tmp/chenxiaoguang_messaging"//第二条管道
#define BUFF_SZ 100
void handler(int sig){
	unlink(FIFO_CMD);
	unlink(FIFO_MSG);
	exit(1);
}

/* 创建守护进程 */
int init_server(int nochdir,int noclose)
{
	pid_t pid;
	pid = fork();
	if (pid < 0) {
		perror("fork");
		return -1;
	}
	if (pid != 0)
		exit(0);
	/* 成为首进程 */
	pid = setsid();
	if (pid < -1)	{
		perror("setsid");
		return -1;
	}
	if (!nochdir)
		chdir("/");
	/* 重定向 */
	if (!noclose) {
		int fd;
		fd = open("/dev/null",O_RDWR,0);
		if (fd!=-1) {
			dup2(fd,STDIN_FILENO);
			dup2(fd,STDOUT_FILENO);
			dup2(fd,STDERR_FILENO);
			if(fd>2)
				close(fd);
		}
	}
	umask(0000);
	return 0;
}

int main(void)
{
	int i, res, flag, userid, sum;
	int fd_msg,fd_server;
	char buffer[BUFF_SZ], user_fifo[BUFF_SZ], txt[BUFF_SZ];
	void *shared_memory = (void *)0;//分配的共享内存的原始首地址
  struct shared_use_st *shared;//指向shm
  int shmid;//共享内存标示符
	pid_t childpid = 0;
	CLIENTINFO clientinfo;
	USER user;

	init_server(0,0);//守护进程

	signal(SIGKILL,handler);
	signal(SIGINT,handler);
	signal(SIGTERM,handler);

	/* 创建共享内存 */
	shmid = shmget((key_t)1234,sizeof(struct shared_use_st),0666|IPC_CREAT);
	if (shmid == -1) {
		fprintf(stderr,"shmget failed\n");
		exit(EXIT_FAILURE);
	}

	/* 将共享内存连接到当前进程的地址空间 */
	shared_memory = shmat(shmid,(void *)0,0);
	if (shared_memory == (void *)-1) {
		fprintf(stderr,"shmat failed\n");
		exit(EXIT_FAILURE);
	}

	/* 设置共享内存 */
	shared = (struct shared_use_st *)shared_memory;

	/* 创建子进程 监听第二个管道 */
	childpid = fork();
	if (childpid == 0) {
		if (access(FIFO_MSG, F_OK) == -1) {
			res = mkfifo(FIFO_MSG, 0777);
			if (res != 0) {
				perror("FIFO_MSG");
				printf("FIFO %s was not created\n",FIFO_MSG);
				exit(EXIT_FAILURE);
			}
		}

		/* 读写第二个管道 */
		fd_msg = open(FIFO_MSG, O_RDONLY);
		if (fd_msg == -1) {
			printf("Could not open %s for read only access\n",FIFO_MSG);
			exit(EXIT_FAILURE);
		}

		while(1) {
			res = read(fd_msg, &clientinfo, sizeof(CLIENTINFO));
			if (res != 0){
				memset(user_fifo, 0, sizeof(user_fifo));//初始化用户信息缓存
				sprintf(buffer, "%s", clientinfo.content);//把客户端要发送的信息放入缓存
				/* 读取命令 */
				if (strcmp(clientinfo.otherfifo, "sendall") == 0) {//发送信息给所有用户
					for (i = 0; i < shared->number; i++) {
						if (shared->online[i] == 1 && strcmp(shared->user_name[i], clientinfo.myfifo) != 0) {
							memset(user_fifo,0,sizeof(user_fifo));//重置用户信息
							sprintf(user_fifo, "/tmp/chat_client%s_fifo", shared->user_name[i]);
							fd_server = open(user_fifo, O_WRONLY | O_NONBLOCK);
							write(fd_server, buffer, strlen(buffer)+1);//把缓存内容写入服务器文件
							close(fd_server);
						}
					}
				}
				else {
					sprintf(user_fifo, "/tmp/chat_client%s_fifo", clientinfo.otherfifo);//发送聊天信息给指定用户
					fd_server = open(user_fifo, O_WRONLY | O_NONBLOCK);
					write(fd_server, buffer, strlen(buffer)+1);
					close(fd_server);
				}
			}
		}
	}
	else {
		/* 读写第一条管道 */
		if (access(FIFO_CMD, F_OK) == -1) {
			res = mkfifo(FIFO_CMD, 0777);
			if (res != 0) {
				perror("FIFO_CMD");
				printf("FIFO %s was not created\n",FIFO_CMD);
				exit(EXIT_FAILURE);
			}
		}

		fd_msg = open(FIFO_CMD, O_RDONLY);
		if (fd_msg == -1) {
			printf("Could not open %s for read only access\n",FIFO_CMD);
			exit(EXIT_FAILURE);
		}

		while(1) {
			/* 读取用户命令 */
			res = read(fd_msg, &user, sizeof(USER));
			if (res != 0) {
				/* 登陆命令 */
				if (strcmp(user.cmd, "login") == 0) {
					/* 初始化缓存与用户信息缓存 */
					memset(buffer, 0, sizeof(buffer));
					memset(user_fifo, 0, sizeof(user_fifo));
					sprintf(user_fifo, "/tmp/login_client%s_fifo", user.name);
					fd_server = open(user_fifo, O_WRONLY | O_NONBLOCK);//读取登陆用管道内信息
					flag = 0;//标志
					for(i = 0 ;i < shared->number; i++) {
						if (strcmp(user.name, shared->user_name[i]) == 0) {
							userid = i;
							if (shared->online[userid] == 1) {//检查是否重名
								flag = -1;
								sprintf(buffer, "n");
								write(fd_server, buffer, strlen(buffer)+1);
							}
							else {
								flag = 1;
								sprintf(buffer, "y"); //登陆成功
								write(fd_server, buffer, strlen(buffer)+1);
								shared->online[userid] = 1;
							}
						}
					}

					if (flag == 0) {//新加入用户
						userid = shared->number;
						sprintf(buffer, "y");
						write(fd_server, buffer, strlen(buffer)+1);
						strcpy(shared->user_name[userid], user.name);
						shared->online[userid] = 1;
						shared->number++;
					}

					close(fd_server);

					/* 新用户登陆提醒 */
					if (flag != -1) {
						memset(buffer, 0, sizeof(buffer));
						sprintf(buffer, "%s 上线了！", user.name);
						for(i = 0; i<shared->number; i++) {
							if (shared->online[i] == 1 && i != userid) {
								memset(user_fifo, 0, sizeof(user_fifo));
								sprintf(user_fifo, "/tmp/chat_client%s_fifo", shared->user_name[i]);
								fd_server = open(user_fifo, O_WRONLY | O_NONBLOCK);
								write(fd_server, buffer, strlen(buffer)+1);
								close(fd_server);
							}
						}
					}
				}

				/* 退出聊天室命令 */
				if (strcmp(user.cmd, "quit") == 0) {
					for(i = 0; i < shared->number; i++){
						if(strcmp(user.name, shared->user_name[i]) == 0){
							userid = i;
						}
					}
					shared->online[userid] = 0;//退出后置状态为0

					/* 用户退出提醒 */
					memset(buffer,0,sizeof(buffer));
					sprintf(buffer, "%s 已经下线了！", user.name);
					for(i = 0; i < shared->number; i++){
						if (shared->online[i] == 1 && i != userid) {
							memset(user_fifo, 0, sizeof(user_fifo));
							sprintf(user_fifo, "/tmp/chat_client%s_fifo", shared->user_name[i]);
							fd_server = open(user_fifo, O_WRONLY | O_NONBLOCK);
							write(fd_server, buffer, strlen(buffer)+1);
							close(fd_server);
						}
					}
				}

				/* 聊天室在线用户列表 */
				if (strcmp(user.cmd, "user_list") == 0) {
					memset(buffer, 0, sizeof(buffer));
					memset(txt, 0, sizeof(buffer));
					sum = 0;
					for(i=0; i<shared->number; i++){
						if (shared->online[i] == 1) {
							sum++;
							strcat(txt, " ");
							strcat(txt, shared->user_name[i]);//联结用户名
						}
					}
					sprintf(buffer, "现在已有%d 个用户在线:", sum);
					strcat(buffer, txt);
					strcat(buffer, ".");

					/* 返回用户信息 */
					memset(user_fifo, 0, sizeof(user_fifo));
					sprintf(user_fifo, "/tmp/chat_client%s_fifo", user.name);
					fd_server = open(user_fifo, O_WRONLY | O_NONBLOCK);
					write(fd_server, buffer, strlen(buffer)+1);
					close(fd_server);
				}
			}
		}
	}

	if(childpid > 0) waitpid(childpid,NULL,0);
	return 0;
}
