#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/*ping通返回0，ping不通一直等待*/

int go_ping(char *svrip)

{
        int time=0;
        while(1)
        {
                pid_t pid;
                if ((pid = vfork()) < 0)
                {
                        printf("\nvfork error\n");
                        return 0;
                }
                else if (pid == 0)
                {       
                        if ( execlp("ping", "ping","-c","1",svrip, (char*)0) < 0)
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
                }else
				{
					printf("ping error\n");
                    return -1;
				}
                if(time++>10 )
                        printf("error\n");
                sleep(1);
        }
        return 0;
}

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
		printf("time:%d\n",time);
		if(time++ == count)
		{
			printf("ping error\n");
			return -1;
		}
        sleep(1);
    }
	return 0;
}
int main()
{
    //go_ping("www.baidu.com");
	ping_status("www.baidu.com","eth0",1);
    return 0;
}