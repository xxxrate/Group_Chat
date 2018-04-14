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
#include "clientinfo.h"//引入客户端信息
#include "user.h"//引入聊天室用户信息

#define FIFO_CMD "/tmp/chenxiaoguang_command"//第一条管道
#define FIFO_MSG "/tmp/chenxiaoguang_messaging"//第二条管道
#define BUFF_SZ 100

char mypipename[BUFF_SZ];//用户私有管道
char passpipename[BUFF_SZ];//登陆用管道

void handler(int sig) {
	unlink(mypipename);
	unlink(passpipename);
	exit(1);
}

void cmd_list() {//输出所有命令的使用方法
  printf("\t[command]:\n");
  printf("\t'quit'：退出聊天室\n");
  printf("\t'sendall': 发送信息给所有用户\n");
  printf("\t'name'+'content':发送信息给指定用户\n");
  printf("\t'user_list':把在线用户列表发送到本机\n");
}


int main(int argc, char *argv[])
{
  int i,res,flag=0;
  pid_t childpid = 0;//子进程初始化
	int fd_server, my_fifo, pass_fifo;//fd_server服务器文件，my_fifo用户文件，pass_fifo登陆用文件
	int fd;//普通文件处理
	char txt1[50], txt2[50], buff;//txt1、txt2分别为第一二次的键盘输入，buff文字处理缓存
	CLIENTINFO clientifo;
	USER user;
	char buffer[BUFF_SZ];

	/* 信号量操控 */
	signal(SIGKILL, handler);
	signal(SIGINT, handler);
	signal(SIGTERM, handler);

	/* 检查第一个公共管道是否存在 */
	if (access(FIFO_CMD, F_OK) == -1) {
		printf("Could not open FIFO %s\n", FIFO_CMD);
		exit(EXIT_FAILURE);
	}
	if (access(FIFO_MSG, F_OK) == -1) {
		printf("Could not open FIFO %s\n", FIFO_MSG);
		exit(EXIT_FAILURE);
	}

	/* 创建用户私有管道 */
	sprintf(mypipename, "/tmp/chat_client%s_fifo", argv[1]);
	if (access(mypipename, F_OK) == -1) {
		res = mkfifo(mypipename, 0777);
		if (res != 0) {
			printf("FIFO %s was not created\n", buffer);
			exit(EXIT_FAILURE);
		}
	}

	/* 打开用户管道进行读写操作 */
	my_fifo = open(mypipename, O_RDONLY | O_NONBLOCK);
	if (my_fifo == -1) {
		printf("Could not open %s for read only access\n", mypipename);
		exit(EXIT_FAILURE);
	}

	/* 创建登陆用管道 */
	sprintf(passpipename, "/tmp/login_client%s_fifo", argv[1]);
	if (access(passpipename, F_OK) == -1){
		res = mkfifo(passpipename,0777);
		if (res != 0){
			printf("FIFO %s was not created\n", passpipename);
			exit(EXIT_FAILURE);
		}
	}

	/* 打开登陆用管道进行读写 */
	pass_fifo = open(passpipename, O_RDONLY | O_NONBLOCK);
	if (pass_fifo == -1){
		printf("Could not open %s for read only access\n", passpipename);
		exit(EXIT_FAILURE);
	}

	/* 打开第一个公共管道进行读写操作 */
	fd_server = open(FIFO_CMD, O_WRONLY);
	if (fd_server == -1) {
		printf("Cound not open %s for write access\n", FIFO_CMD);
		exit(EXIT_FAILURE);
	}

	/* 构建聊天室用户信息 */
	strcpy(user.name, argv[1]);
	strcpy(user.cmd, "login");

	/* 把该用户信息写入服务器文件 */
	write(fd_server, &user, sizeof(USER));
	close(fd_server);

	/* 缓存初始化 */
	memset(buffer,'\0',BUFF_SZ);

	/* 从登陆用管道读取文件，检查登陆状态 */
	while(1){
		res = read(pass_fifo, buffer, BUFF_SZ);
		if(res > 0) {
			unlink(passpipename);
			if(strcmp(buffer, "y") == 0){
				printf("\033[25C用户%s 登陆成功！\n", argv[1]);
				break;
			}
			else{
				printf("\033[10C对不起，该用户名%s 已经被人使用了,请使用其他用户名\n", argv[1]);
				return 1;
			}
		}
	}

	/* 创建子进程  */
	childpid = fork();

	/* 子进程进行操作 */
	if (childpid == 0){
		while(1) {
			memset(buffer,'\0',BUFF_SZ);
		  res = read(my_fifo, buffer, BUFF_SZ);
			if (res > 0) {
				if (strcmp(buffer, "y") != 0 && strcmp(buffer, "n") != 0){
					printf("%s\n", buffer);
				}
			}
		}
	}
	else {//登陆成功后，子进程进行对文件读写操作
		while(1){
			/* 初始化变量 */
			memset(txt1,0,sizeof(txt1));
			memset(txt2,0,sizeof(txt2));
			flag = 0;//标志
			i = 0;

			/* 键盘输入 */
			while((buff = getchar())!='\n'){
				if (isspace(buff) && flag == 0){
					flag = 1;
					i = 0;
				}
				else if (flag == 0){
					txt1[i] = buff;
					i++;
				}
				else if (flag == 1){
					txt2[i] = buff;
					i++;
				}
 			}
			/* 光标控制 */
			printf("\033[1A");//光标向上移动一行
			printf("\033[K");//清除光标后的内容

			/* 判断命令然后进行操作*/
			if (strcmp(txt1, "help") == 0){//键盘输入help
				cmd_list();
			}
			else if (strcmp(txt1, "quit") == 0 || strcmp(txt1, "user_list") == 0){//键盘输入quit、user_list
				/* 打开服务器文件进行读写操作 */
				fd_server = open(FIFO_CMD, O_WRONLY);
				if (fd_server == -1) {
					printf("Cound not open %s for write access\n",FIFO_CMD);
					exit(EXIT_FAILURE);
				}

				/* 构建用户信息与用户使用命令 */
				strcpy(user.name, argv[1]);
				strcpy(user.cmd, txt1);

				/* 把以上操作写入服务器文件 */
				write(fd_server, &user, sizeof(USER));
				close(fd_server);

				/* 退出聊天室 */
				if (strcmp(txt1, "quit") == 0){
					(void)unlink(mypipename);
					printf("退出成功！\n");
					return 0;
				}
			}
			else if (txt1[0] != '\0' && txt2[0] != '\0'){
				printf("\033[40C发送信息到 %s : %s\n", txt1, txt2);

				/* 读写服务器文件 */
				fd_server = open(FIFO_MSG, O_WRONLY);
				if (fd_server == -1) {
					printf("Cound not oopen %s for write accsee\n",FIFO_MSG);
					exit(EXIT_FAILURE);
				}

				/* 构建客户端信息与用户间聊天信息 */
				strcpy(clientifo.myfifo, argv[1]);
				sprintf(clientifo.otherfifo, "%s", txt1);
				sprintf(clientifo.content, "%s 发送给你 : %s", argv[1], txt2);

				/* 以上操作写入服务器文件 */
				write(fd_server, &clientifo, sizeof(CLIENTINFO));
				close(fd_server);
			}
		}
        }

	if(childpid > 0) waitpid(childpid,NULL,0);//终结子进程

	/* 从系统删除用户私用管道 */
	close(my_fifo);
	(void)unlink(mypipename);

	exit(0);
}
