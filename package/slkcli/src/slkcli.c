#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<termios.h>
#include<errno.h>   
#include <string.h>     
#include <time.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  

 
#define FALSE -1
#define TRUE 0
#define BUFSIZE 512
#define MAXLINE 512 
#define KEYVALLEN 100


int speed_arr[] = { B115200, B57600, B38400, B19200, B9600, B4800, B2400, B1200, B600, B300};
int name_arr[] = { 115200, 57600, 38400,  19200,  9600,  4800,  2400,  1200, 600, 300};
int fd1;
fd_set rfds;
struct timeval tv;
int nread;
//char buff[512];
char dev[100] ={"/dev/ttyS0"};
//char buff1[1024];

pthread_t thread[3];
int islinkerror;
char sendline[100] ="tcplink";
int outlen = 0;
int t1=5;
struct sockaddr_in servaddr;  
//char buf[MAXLINE];  
socklen_t cliaddr_len; 
int sockfd, n,connfd,ret;  
char recvline[MAXLINE]; 
char server_add[100]={"192.168.0.123"};
short listenPort=8000;

void set_speed(int fd, int speed) 
{
	int i;
	int status;
	struct termios   Opt;
	tcgetattr(fd, &Opt);
	for (i = 0; i < sizeof(speed_arr) / sizeof(int); i++) {
		if (speed == name_arr[i]) {
			tcflush(fd, TCIOFLUSH);
			cfsetispeed(&Opt, speed_arr[i]);
			cfsetospeed(&Opt, speed_arr[i]);
			status = tcsetattr(fd, TCSANOW, &Opt);
			if (status != 0) {
				perror("tcsetattr fd1");
				return;
			}
			tcflush(fd, TCIOFLUSH);
		}
	}
}

int set_Parity(int fd, int databits, int stopbits, int parity)
{
	//printf("%d",parity);
	struct termios options;
	if (tcgetattr(fd, &options) != 0) {
		perror("SetupSerial 1");
		return(FALSE);
	}
	options.c_cflag &= ~CSIZE;
	switch (databits)
	{
	case 7:
		options.c_cflag |= CS7;
		break;
	case 8:
		options.c_cflag |= CS8;
		break;
	default:
		fprintf(stderr, "Unsupported data size\n"); return (FALSE);
	}
	switch (parity)
	{
	case 'n':
	case 'N':
		options.c_cflag &= ~PARENB;   //Clear parity enable 
		options.c_iflag &= ~INPCK;     // Enable parity checking 
		break;
	case 'o':
	case 'O':
		options.c_cflag |= (PARODD | PARENB);
		options.c_iflag |= INPCK;             // Disnable parity checking 
		break;
	case 'e':
	case 'E':
		options.c_cflag |= PARENB;     // Enable parity 
		options.c_cflag &= ~PARODD;
		options.c_iflag |= INPCK;       // Disnable parity checking 
		break;
	case 'S':
	case 's':  //as no parity
		options.c_cflag &= ~PARENB;
		options.c_cflag &= ~CSTOPB; break;
	default:
		fprintf(stderr, "Unsupported parity\n");
		return (FALSE);
	}

	switch (stopbits)
	{
	case 1:
		options.c_cflag &= ~CSTOPB;
		break;
	case 2:
		options.c_cflag |= CSTOPB;
		break;
	default:
		fprintf(stderr, "Unsupported stop bits\n");
		return (FALSE);
	}
	// Set input parity option 
	if (parity != 'n')
		options.c_iflag |= INPCK;
	//c_cc数组的VSTART和VSTOP元素被设定成DC1和DC3，代表ASCII标准的XON和XOFF字符，如果在传输这两个字符的时候就传不过去，需要把软件流控制屏蔽
	options.c_iflag &= ~ (IXON | IXOFF | IXANY); 
	//在用write发送数据时没有键入回车，信息就发送不出去，这主要是因为我们在输入输出时是按照规范模式接收到回车或换行才发送，而更多情况下我们是不必键入回车或换行的。此时应转换到行方式输入，不经处理直接发送，设置如下
	options.c_iflag &= ~ (INLCR | ICRNL | IGNCR);
	//还存在这样的情况：发送字符0X0d的时候，往往接收端得到的字符是0X0a，原因是因为在串口设置中c_iflag和c_oflag中存在从NL-CR和CR-NL的映射，即串口能把回车和换行当成同一个字符，可以进行如下设置屏蔽之：
	options.c_oflag &= ~(ONLCR | OCRNL);
	options.c_lflag &= ~ (ICANON | ECHO | ECHOE | ISIG);
	
	tcflush(fd, TCIFLUSH);
	options.c_cc[VTIME] = 150;
	options.c_cc[VMIN] = 0; // Update the options and do it NOW 
	if (tcsetattr(fd, TCSANOW, &options) != 0)
	{
		perror("SetupSerial 3");
		return (FALSE);
	}
	return (TRUE);
}



void serial()
{
	
	while(1)
	{
		unsigned char buff[BUFSIZE];
		memset(buff,0,sizeof(buff));
		FD_ZERO(&rfds);
		FD_SET(fd1, &rfds);
		if (select(1+fd1, &rfds, NULL, NULL, &tv)>0)
		{
           if (FD_ISSET(fd1, &rfds))
           {
              nread=read(fd1, buff, BUFSIZE);
			  
			  //printf("%s\n",buff);
			  send(sockfd,buff,nread,0);
           }
		}
	}
	/*int ret1;
	while(1)
	{
		char buff[MAXLINE];
		memset(buff,0,strlen(buff));
		ret1 = selectlidar(fd1,30,0);
		if(ret1 > 0)
		{
			//sleep(2);
			nread = read(fd1, buff, MAXLINE);
			//buff[nread]='\0';
			//printf("%s\n",buff);
			send(sockfd, buff, strlen(buff),0);
		}
		memset(buff,0,strlen(buff));
	}*/
	
}


int selectlidar( int fd, int sec, int usec)
{
	//int ret;
	fd_set fds;
	struct timeval timeout;
 
	timeout.tv_sec = sec;
	timeout.tv_usec = usec;
 
	FD_ZERO(&fds); //每次循环都要清空集合，否则不能检测描述符变化
	FD_SET(fd,&fds); //添加描述符
 
	ret = select(fd+1,&fds,&fds,NULL,&timeout);
	if(0 > ret)
	{
		printf("lidar recv select error\n");
        	return -1;	
	}
	else if (ret == 0)
	{
	        printf("uart read select timeout\n");
	        return 0;
	}
    	else {
		if(FD_ISSET(fd,&fds)) //测试sock是否可读，即是否网络上有数据
		{
			return 1;
 
		}
		else
		{
			return 0;
		}
    	}
}

void sendheartbeat()
{
	while(1)
	{
		int i;
		i=send(sockfd,sendline,outlen,0);
		if(i<0)
			islinkerror=1;
		else
			islinkerror=0;	
		sleep(t1);
	}
}

int StringToHex(char* str, unsigned char* out, int* outlen)
{
	char* p = str;
	char high = 0, low = 0;
	int tmplen = strlen(p), cnt = 0;
	tmplen = strlen(p);
	while (cnt < (tmplen / 2))
	{
		high = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
		low = (*(++p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *(p)-48 - 7 : *(p)-48;
		out[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
		p++;
		cnt++;
	}
	if (tmplen % 2 != 0) out[cnt] = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;

	if (outlen != NULL) *outlen = tmplen / 2 + tmplen % 2;
	return tmplen / 2 + tmplen % 2;
}

void tcpclient()
{
	while(1)
	{	
		unsigned char buf[MAXLINE];
		//unsigned char buf1[MAXLINE];
		memset(buf,0,sizeof(buf));
		int ret1 = selectlidar(sockfd,1,0);
		if(ret1 > 0)
		{
			n = recv(sockfd, buf, MAXLINE,0);
			//buf[n]='\0';
			
			write(fd1, buf, n);
		
		}
		if(ret1==1 && n==0)
		{
			islinkerror=1;
		}
		else
		{
			islinkerror=0;
		}
		//memset(buf,0,strlen(buf));
	}
}



int main(int argc,char *argv[])
{
	int i=0;
	int Bound=9600;
	int databits = 8;
	int stopbits = 1;
	int parity;
	char parity1[10]="EVEN";
	if(strcmp(parity1,"NONE")==0)
		parity=78;
	else if(strcmp(parity1,"ODD")==0)
		parity=79;
	else if(strcmp(parity1,"EVEN")==0)
		parity=69;
	else
		parity=78;
	unsigned char out[1024];
	
	int opt;
    while ((opt=getopt(argc,argv,"c:s:b:d:o:x:i:p:t:h:e:?")) != -1)
    {
			if(opt=='c')
			{
			char profile[100];
			memset(profile,0,sizeof(profile));
			strncpy(profile,optarg,100);
			FILE* fp;
				if ((fp = fopen(profile, "r")) == NULL) {
					printf("openfile [%s] error [%s]\n", profile, strerror(errno));
					break;
				}
			
			fseek(fp,0L,SEEK_END);  //定位到文件末尾
			int flen = ftell(fp); //得到文件大小
			char *p = (char *)malloc(flen+1); //分配空间存储文件中的数据
			if(p == NULL)
			{
				fclose(fp);
				return 0;
			}
			fseek(fp,0L,SEEK_SET); //定位到文件开头
			fread(p,flen,1,fp);  //一次性读取全部文件内容
			p[flen] = '\0';  // 字符串最后一位为空
			printf("file flen is %d\n\n",flen);
			printf("read file buff is %s\n",p);
			char tmp[10][40];
			char str0[] = " ";  
			char *result = NULL;  
			result = strtok( p, str0 );  
			printf("%s\n",result);  
			
			while( result != NULL )
			{  
				// printf("%s ", result);  
				strcpy(tmp[i], result);  
				//printf("%s^",tmp[i]);  
				result = strtok(NULL, str0);  
				i++;
			}  
			memset(dev,0,sizeof(dev));
			strncpy(dev,tmp[0],99);
			Bound=atoi(tmp[1]);
			databits=atoi(tmp[2]);
			stopbits=atoi(tmp[3]);
			//parity=78;
			//printf("%s\n",tmp[4]);
			if(strcmp(tmp[4],"NONE")==0)
			{
				parity=78;
				//printf("parity:%d\n",parity);
			}
			else if(strcmp(tmp[4],"ODD")==0)
				parity=79;
			else if(strcmp(parity1,"EVEN")==0)
				parity=69;
			else
				parity=78;
			//tmp[4];
			//printf("%d\n",parity);
			memset(server_add,0,sizeof(server_add));
			strncpy(server_add,tmp[5],99);
			//server_add=tmp[5];
			listenPort=atoi(tmp[6]);
			t1=atoi(tmp[7]);
			memset(sendline,0,sizeof(sendline));
			strncpy(sendline,tmp[8],99);
			printf("heartbeat:%s\n", sendline);
			if (strcmp(tmp[9], "1") == 0)
			{
				printf("hex the heartbeat:\n");
				StringToHex(sendline, out, &outlen);
				//strncpy(sendline, out, 99);
				int cnt;
				for (cnt = 0; cnt < outlen; cnt++)
				{
					printf("%02X ", out[cnt]);
					sendline[cnt] = out[cnt];
				}
				printf("\n");
				for (cnt = 0; cnt < outlen; cnt++)
				{
					
					printf("%02X ", sendline[cnt]);
				}
			}
			else {
				outlen = strlen(sendline);
			}
			//sendline=tmp[8];
			break;
		}
		else
		{
			switch (opt)
			{
				case 's':
					memset(dev,0,sizeof(dev));
					strncpy(dev,optarg,99);
					break;
				case 'b':
					Bound = atoi(optarg);
					break;
				case 'd':
					databits = atoi(optarg);
					break;
				case 'o':
					stopbits = atoi(optarg);
					break;
				case 'x':
					memset(parity1,0,sizeof(parity1));
					strncpy(parity1,optarg,10);
					break;
				case 'i':
					memset(server_add,0,sizeof(server_add));
					strncpy(server_add,optarg,99);
					break;
				case 'p':
					listenPort = atoi(optarg);
					break;
				case 't':
					t1 = atoi(optarg);
					break;
				case 'h':
					memset(sendline,0,sizeof(sendline));
					strncpy(sendline,optarg,99);
					
					break;
				case 'e':
					//心跳包转码
					//transheartbeat();
					StringToHex(sendline, out, &outlen);
					strncpy(sendline, out, 99);
					break;
				default: /* '?' */
					fprintf(stderr, "Usage: %s [-s serial_port -b baund -d databits -o stopbits -x parity -i tcpserver_address -p tcp_port -t hearttime -h heartbeat -e hexheartbeat]  \n",argv[0]);
					exit(EXIT_FAILURE);
        }
		}
    }
	
	fd1 = open(dev, O_RDWR);
	if (fd1 == -1)
	{
		perror("serialport error\n");
	}
	else
	{
		printf("open ");
		//printf("%s", ttyname(fd1));
		printf(" succesfully\n");
	}

	set_speed(fd1, Bound);
	if (set_Parity(fd1, databits, stopbits, parity) == FALSE) {
		printf("Set Parity Error\n");
		exit(0);
	}
	tv.tv_sec=30;
	tv.tv_usec=0;
	
	
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    bzero(&servaddr, sizeof(servaddr));  
    servaddr.sin_family = AF_INET;   
    servaddr.sin_addr.s_addr = inet_addr(server_add);
    servaddr.sin_port = htons(listenPort);  
	//connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	
	while(1)
	{
		if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))==-1)
		{
			//close(sockfd);
			printf("连接服务端失败\n");
			printf("5s后将重新连接服务器\n");
			sleep(5);
			sockfd = socket(AF_INET, SOCK_STREAM, 0); 
			bzero(&servaddr, sizeof(servaddr));  
			servaddr.sin_family = AF_INET;   
			servaddr.sin_addr.s_addr = inet_addr(server_add);
			servaddr.sin_port = htons(listenPort);  
			islinkerror=1;
		}
		else
		{
			islinkerror=0;
			printf("连接服务器成功\n");
			if(i>7 || argc> 7)
			{
				pthread_create(&thread[0],NULL,(void *)sendheartbeat,NULL);
			}
			pthread_create(&thread[1],NULL,(void *)tcpclient,NULL);
			pthread_create(&thread[2],NULL,(void *)serial,NULL);
			break;
		}
	}
	
	while(1)
	{
		//sleep(10);
		if(islinkerror==1)
		{
			//sleep(5);
			
			printf("连接服务端失败\n");
			printf("5s后将重新连接服务器\n");
			sleep(5);
			sockfd = socket(AF_INET, SOCK_STREAM, 0); 
			bzero(&servaddr, sizeof(servaddr));  
			servaddr.sin_family = AF_INET;   
			servaddr.sin_addr.s_addr = inet_addr(server_add);
			servaddr.sin_port = htons(listenPort); 
			if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))==-1)
			{
				islinkerror=1;
				printf("服务器未启动，客户端取消连接。\n");
				sleep(5);
			}
			else
			{
				printf("连接服务器成功\n");
				islinkerror=0;
				sleep(5);
			}

		}
	}
	return 0;
}





