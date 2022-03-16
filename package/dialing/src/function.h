#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include <termios.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <netinet/ip.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h> 
#include <stdbool.h>

int flag_debug = 1;
int value_signal = 0;
int flag_ttyUSB = 0;
char value_module[100];
char value_ifname[100];
char flag_module[5];
char name_config[10];
char path_usb[100];
char path_at[100] = "/tmp/at_";
char path_outfile[100] = "/tmp/tmp_";
char path_formfile[100] = "/tmp/";
char path_debuglog[100] = "/etc/debug_";
char tmp_debuglog[500] = "/etc/tmp_debug_";
char cmd_ifup[100] = "ifup ";
char cmd_ifdown[100] = "ifdown ";
int count_loop=0;
int speed_arr[] = { B115200, B9600, };    
int name_arr[] = {115200, 9600, };
/* 功能：读取命令返回值 
** 传参：cmd 命令；respond 返回结果
** 返回值：void
*/
void cmd_recieve_str(char *cmd, char *respond)
{
    FILE * p_file = NULL;
	char msg[1024];
	memset(msg,0,sizeof(msg));
	char tmp_cmd[1024];
	memset(tmp_cmd,0,sizeof(tmp_cmd));
	int len = 0;
	// 使用popen函数则用pclose 关闭文件
	sprintf(tmp_cmd,"%s | tr -d '\n'",cmd);
    if((p_file=popen(tmp_cmd,"r"))==NULL)
    {
        perror("popen error");
        //return -2;
    }
    else
    {
        while(fgets(msg,sizeof(msg),p_file)!=NULL)
        {
            //printf("msg:%s",msg);
			if(respond != NULL)
			{
				//printf("msg:%s",msg);
				strncat(respond,msg,sizeof(msg));
			}
        }
		if(respond != NULL)
			respond[strlen(respond)]='\0';
		//printf("respond:%s",respond);
    }
	pclose(p_file);
	/* printf("sizeof respond:%d\n",sizeof(respond));
	if(sizeof(respond)>0)
	{
		printf("respond:%s\n",respond);
	}else{
		printf("no respond\n");
	} */
}

/* 功能：读取命令返回值
** 传参：cmd 命令
** 返回值：int
*/
int cmd_recieve_int(char * cmd)
{
    FILE * p_file = NULL;
	char msg[1024];
	memset(msg,0,sizeof(msg));
	// 使用popen函数则用pclose 关闭文件
    if((p_file=popen(cmd,"r"))==NULL)
    {
        perror("popen error");
        //return -2;
    }
    else
    {
        while(fgets(msg,sizeof(msg),p_file)!=NULL)
        {
			//printf("msg:%s\n",msg);
			//strcat(msg,msg);
        }
       
    }
	pclose(p_file);
	
	//printf("strlen respond:%d\n",strlen(msg));
	if(strlen(msg)>0)
	{
		//printf("respond:%s\n",msg);
		msg[strlen(msg)-1]='\0';
		return atoi(msg);
	}else{
		//printf("no respond\n");
		return -1;
	}
}

/* 功能：获取文件大小
** 传参：path /路径/文件名
** 返回值：long int
*/ 
unsigned long get_file_size(char * path)
{  
    unsigned long filesize = -1;      
    struct stat statbuff;  
    if(stat(path, &statbuff) < 0){  
        return filesize;  
    }else{  
        filesize = statbuff.st_size;  
    }  
    return filesize;  
}

/* 功能：打印日志
** 传参：str 内容；head 项名
** 返回值：void
*/ 
void Debug(char *str,char *head)
{
	if( flag_debug == 0 ) //不打印日志
		return;
	char time[30];
	char tmp_ptr1[1024];
	char tmp_ptr2[1024];
	memset(time,0,sizeof(time));
	memset(tmp_ptr1,0,sizeof(tmp_ptr1));
	memset(tmp_ptr2,0,sizeof(tmp_ptr2));
	//日志格式：系统时间（2021-11-03 16:17:32）项（APN：）内容（3gnet）
	if( head != NULL && strlen(head) > 0 ) //存在项名且不为空
		sprintf(tmp_ptr1,"%s:%s",head,str);
	else //不存在项名，直接打印时间和内容
		strcpy(tmp_ptr1,str);
	cmd_recieve_str("date \"+%Y-%m-%d %H:%M:%S\" | xargs echo -n", time); //获取系统时间
	sprintf(tmp_ptr2, "%s %s",time, tmp_ptr1);
	if ( flag_debug == 1 ) //后台显示日志
	{
		printf("%s\n", tmp_ptr2);
	}
	else if( flag_debug == 2 ) //将日志输出到/etc/debug文件中
	{
		char tmp_ptr[1024];
		memset(tmp_ptr,0,sizeof(tmp_ptr));
		
		sprintf(tmp_ptr,"%s%s%s%s","echo \"",tmp_ptr2,"\">>",path_debuglog);
		system(tmp_ptr);
		if ( get_file_size(path_debuglog) > 100000 ){
			char tmp_tail[200]; //tail命令
			//将/etc/debug文件的最后十行复制到/etc/tmp_debug文件
			memset(tmp_tail,0,sizeof(tmp_tail));
			sprintf(tmp_tail,"%s %s %s %s","tail -n 10",path_debuglog,">",tmp_debuglog);
			system(tmp_tail);
			//删除/etc/debug文件
			memset(tmp_tail,0,sizeof(tmp_tail));
			sprintf(tmp_tail,"%s %s","rm",path_debuglog);
			system(tmp_tail);
			//将/etc/tmp_debug文件复制到/etc/debug
			memset(tmp_tail,0,sizeof(tmp_tail));
			sprintf(tmp_tail,"%s %s %s","mv",tmp_debuglog,path_debuglog);
			system(tmp_tail);
		}
	}
}
/*-----------------------------------------------------------------------------  
  函数名:      set_parity  
  参数:        int fd  
  返回值:      int  
  描述:        设置fd表述符的奇偶校验  
 *-----------------------------------------------------------------------------*/    
int set_parity(int fd)    
{    
    struct termios opt;    
    
    if(tcgetattr(fd,&opt) != 0)                 //或许原先的配置信息     
    {    
        perror("Get opt in parity error:");    
        return -1;    
    }    
    
    /*通过设置opt数据结构，来配置相关功能，以下为八个数据位，不使能奇偶校验*/    
    opt.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP    
                | INLCR | IGNCR | ICRNL | IXON);    
    opt.c_oflag &= ~OPOST;    
    opt.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);    
    opt.c_cflag &= ~(CSIZE | PARENB);    
    opt.c_cflag |= CS8;    
    
    tcflush(fd,TCIFLUSH);                           //清空输入缓存     
    
    if(tcsetattr(fd,TCSANOW,&opt) != 0)    
    {    
        perror("set attr parity error:");    
        return -1;    
    }    
    
    return 0;    
}
/*-----------------------------------------------------------------------------  
  函数名:      set_speed  
  参数:        int fd ,int speed  
  返回值:      void  
  描述:        设置fd表述符的串口波特率  
 *-----------------------------------------------------------------------------*/    
void set_speed(int fd ,int speed)    
{    
    struct termios opt;    
    int i;    
    int status;    
    
    tcgetattr(fd,&opt);    
    for(i = 0;i < sizeof(speed_arr)/sizeof(int);i++)    
    {    
        if(speed == name_arr[i])                        //找到标准的波特率与用户一致     
        {    
            tcflush(fd,TCIOFLUSH);                      //清除IO输入和输出缓存     
            cfsetispeed(&opt,speed_arr[i]);         //设置串口输入波特率     
            cfsetospeed(&opt,speed_arr[i]);         //设置串口输出波特率     
    
            status = tcsetattr(fd,TCSANOW,&opt);    //将属性设置到opt的数据结构中，并且立即生效     
            if(status != 0)    
                perror("tcsetattr fd:");                //设置失败     
            return ;    
        }    
        tcflush(fd,TCIOFLUSH);                          //每次清除IO缓存     
    }    
} 
/*-----------------------------------------------------------------------------  
  函数名:      serial_init  
  参数:        char *dev_path,int speed,int is_block  
  返回值:      初始化成功返回打开的文件描述符  
  描述:        串口初始化，根据串口文件路径名，串口的速度，和串口是否阻塞,block为1表示阻塞  
 *-----------------------------------------------------------------------------*/   
int serial_init(char *dev_path,int speed,int is_block)    
{    
    int fd;    
    int flag;    
    
    flag = 0;    
    flag |= O_RDWR;                     //设置为可读写的串口属性文件     
    if(is_block == 0)    
        flag |=O_NONBLOCK;              //若为0则表示以非阻塞方式打开     
    
    fd = open(dev_path,flag);               //打开设备文件     
    if(fd < 0)    
    {    
        perror("Open device file err:");    
        close(fd);    
        return -1;    
    }    
    
    /*打开设备文件后，下面开始设置波特率*/    
    set_speed(fd,speed);                //考虑到波特率可能被单独设置，所以独立成函数     
    
    /*设置奇偶校验*/    
    if(set_parity(fd) != 0)    
    {    
        perror("set parity error:");    
        close(fd);                      //一定要关闭文件，否则文件一直为打开状态     
        return -1;    
    }    
    
    return fd;    
}
/*-----------------------------------------------------------------------------  
  函数名:      serial_send  
  参数:        int fd,char *str,unsigned int len  
  返回值:      发送成功返回发送长度，否则返回小于0的值  
  描述:        向fd描述符的串口发送数据，长度为len，内容为str  
 *-----------------------------------------------------------------------------*/    
int serial_send(int fd,char *str,unsigned int len)    
{    
    int ret;    
    
    if(len > strlen(str))                    //判断长度是否超过str的最大长度     
        len = strlen(str);    
    
    ret = write(fd,str,len);    
    if(ret < 0)    
    {    
        perror("serial send err:");    
        return -1;    
    }    
    
    return ret;    
}    
    
/*-----------------------------------------------------------------------------  
  函数名:      serial_read  
  参数:        int fd,char *str,unsigned int len,unsigned int timeout  
  返回值:      在规定的时间内读取数据，超时则退出，超时时间为ms级别  
  描述:        向fd描述符的串口接收数据，长度为len，存入str，timeout 为超时时间  
 *-----------------------------------------------------------------------------*/    
int serial_read(int fd, char *str, unsigned int len, unsigned int timeout)    
{    
    fd_set rfds;    
    struct timeval tv;    
    int ret;                                //每次读的结果     
    int sret;                               //select监控结果     
    int readlen = 0;                        //实际读到的字节数     
    char * ptr;    
    
    ptr = str;                          //读指针，每次移动，因为实际读出的长度和传入参数可能存在差异     
    
    FD_ZERO(&rfds);                     //清除文件描述符集合     
    FD_SET(fd,&rfds);                   //将fd加入fds文件描述符，以待下面用select方法监听     
    
    /*传入的timeout是ms级别的单位，这里需要转换为struct timeval 结构的*/    
    tv.tv_sec  = timeout / 1000;    
    tv.tv_usec = (timeout%1000)*1000;    
    
    /*防止读数据长度超过缓冲区*/    
    //if(sizeof(&str) < len)     
    //  len = sizeof(str);     
    
    
    /*开始读*/    
    while(readlen < len)    
    {    
        sret = select(fd+1,&rfds,NULL,NULL,&tv);        //检测串口是否可读     
    
        if(sret == -1)                              //检测失败     
        {    
            perror("select:");    
            break;    
        }    
        else if(sret > 0)                         
        {    
            ret = read(fd,ptr,1);    
            if(ret < 0)    
            {    
                perror("read err:");    
                break;    
            }    
            else if(ret == 0)    
                break;    
    
            readlen += ret;                             //更新读的长度     
            ptr     += ret;                             //更新读的位置     
        }    
        else                                                    //超时     
        {    
          //  printf("timeout!\n");    
            break;    
        }    
    }    
    
    return readlen;    
}
int sendat(char *at, unsigned int timeout,char *respond)
{
    int fd;
    char buf[1024]; 
	char att[100];
	int slen=0;
	memset(att,0,sizeof(att));
	memset(buf,0,sizeof(buf));
	memset(respond,0,sizeof(respond));
	
    fd =  serial_init(path_usb,115200,1);
    if(fd < 0)    
    {    
        //perror("serial init err:");    
        return -1;    
    }     
	strcat(att,at);
	strcat(att,"\r");
	slen=strlen(att);
    serial_send(fd,att,slen);       
    serial_read(fd,buf,sizeof(buf),timeout);    
	//printf("%s",buf);
	if(respond != NULL)
		strcpy(respond,buf);
    close(fd);
	return 1; 
}

/* 功能：路径补全
** 返回值：void
*/
void fillpath()
{
	strcat(path_at,name_config);
	strcat(path_outfile,name_config);
	strcat(path_formfile,name_config);
	strcat(path_debuglog,name_config);
	strcat(tmp_debuglog,name_config);
	strcat(cmd_ifup,name_config);
	strcat(cmd_ifdown,name_config);
	Debug(path_outfile,"");
	Debug(path_formfile,"");
}

/* 功能：判断字符串是否纯数字
** 传参：str 被判断字符串
** 返回值：int
** 返回结果：0 纯数字；-1 非纯数字
*/
int isInt(char* str)
{
	int len;
	len = strlen(str);
	//printf("len:%d\n",len);
	int i=0;
	for(i;i<len-1;i++)
	{
		if(!isdigit(str[i]))
			return -1;
	}
	return 0;
}
/* 功能：获取ping状态
** 传参：ip 目标IP；ifname 指定网卡;count PING周期
** 返回值：int
** 返回结果：0 ping成功；-1 ping失败
*/
int ping_status(char *svrip,char *ifname,int count)

{
    int time=1;
    while(1)
    {
        pid_t pid;
        if((pid = vfork())<0)
        {
            printf("\nvfork error\n");
            return 0;
        }
        else if(pid == 0)
        {       
			//if(execlp("ping", "ping","-c","1",svrip, (char*)0) < 0)
			if(execlp("ping", "ping","-c","1","-w","1","-I",ifname,svrip, (char*)0) < 0)
			{
				printf("\nexeclp error\n");
				return 0;
			}
		}
        int stat;
		waitpid(pid, &stat, 0);
		if (stat == 0)
		{
			printf("ping ok\n");
			return 0;
		}
		if(time++ == count)
		{
			printf("ping error\n");
			return -1;
		}
        sleep(1);
    }
	return 0;
}

/*循环延时*/
void func_loop()
{
	char str[50];
	memset(str,0,sizeof(str));
	sprintf(str,"While loop %d",count_loop);
	Debug(str,NULL);
	count_loop++;
}
/* 功能：获取子字符串数量
** 传参：father 父字符串child 子字符串
** 返回值：int
** 返回结果：子字符串出现次数
*/
int countstr(char *father,char *child)
{
	int count=0;
	int len_father=strlen(father);
	int len_child=strlen(child);
	while(*father != '\0')
	{
		if(strncmp(father,child,len_child)==0)
		{
			count=count+1;
			father=father+len_child;
		}else{
			father++;
		}
	}
	return count;
}
/*分割字符串*/
int separate_string2array(char *father,char *child,unsigned int num,unsigned int size,char *respond)
{
	char str[1024]={0};
	memset(&str,0x0,sizeof(str));
	memcpy(str,father,strlen(father)); 
	char *token=NULL,*token_saveptr;
	unsigned int count=0;
	token=strtok_r(str,child,&token_saveptr);
	while(token!=NULL)
	{
		memcpy(respond+count*size,token,size);
		if(++count>=num)
			break;
		token=strtok_r(NULL,child,&token_saveptr);
	}
	
	return count;
}
/*将所有ttyUSB排序*/
void sort_ttyUSB(char array[][3],int num)
{
	int i,j;
	int tmp_f;
	int tmp_s;
	for(j=0;j<num-1;j++)
	{
		for(i=0;i<num-j-1;i++)
		{
			tmp_f=atoi(array[i]);
			tmp_s=atoi(array[i+1]);
			if(tmp_f>tmp_s)
			{
				sprintf(array[i],"%d",tmp_s);
				sprintf(array[i+1],"%d",tmp_f);
			}
		}
	}
}
/*检验正确的ttyUSB*/
int check_ttyUSB(char *buff)
{
	int i=countstr(buff,"ttyUSB");
	char array_ttyUSB[i][3];
	char cmd[1024];
	if(separate_string2array(buff,"ttyUSB",i,3,(char *)&array_ttyUSB) != i)
	{
		Debug("Format ttyUSB error!",NULL);
		return -1;
	}
	int j,count_out=0;
	sort_ttyUSB(array_ttyUSB,i);
	char tmp_buff[200];
	while(!strstr(tmp_buff,"OK"))
	{
		for(j=0;j<i;j++)
		{
			memset(path_usb,0,sizeof(path_usb));
			sprintf(path_usb,"/dev/ttyUSB%s",array_ttyUSB[j]);
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"sendat %s ATI",path_usb);
			memset(tmp_buff,0,sizeof(tmp_buff));
			cmd_recieve_str(cmd,tmp_buff);
			int count;
			for(count=0;count<2;count++)
				if(strstr(tmp_buff,"OK"))
				{
					
					Debug(path_usb,"AT PATH");
					return 0;
				}
		}
		if(count_out==5)
		{	
			Debug("failed!","AT PATH");
			exit(1);
		}
		count_out++;
	}
	Debug("failed!","AT PATH");
	return 0;
}
