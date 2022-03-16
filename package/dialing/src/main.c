#include "function.h"
#include "fm650.h"
#include "rm500u.h"
#include "sim7600.h"
#include "sim8200.h"
#include "dialing.h"

int main(int argc, char *argv[])
{
	/*补全文件路径*/
	//printf("argc:%d",argc);
	if(argc==2)
	{
		
		/*多模组拨号*/
		strcpy(name_config,argv[1]);
		if(strstr(name_config,"SIM3"))
			strcpy(flag_module,"1-1.1");
		else if(strstr(name_config,"SIM1"))
			strcpy(flag_module,"2-1.2");
		else if(strstr(name_config,"SIM2"))
			strcpy(flag_module,"3-1.1");
		else if(strstr(name_config,"SIM4"))
			strcpy(flag_module,"3-1.2");
		Debug(flag_module,"Multiple modules dialing runing, Module");
		fillpath();
	}
	else
	{
		/*单模组拨号*/
		Debug("Single module dialing runing",NULL);
		strcpy(name_config,"SIM");
		strcpy(flag_module,"2-1");
		fillpath();
	}
	/*读modem配置文件*/
	readConfig();
	/*获取模组信息*/
	getModule();
	/*设置模组拨号方式*/
	setModule();
	while(1)
	{
		/*读取驻网信息*/
		getModem();
		/*设置拨号信息*/
		setModem();
		/*拨号*/
		Dial();
		/*信号灯控制*/
		controlLED();
		/*网络监测*/
		checkDNS();
		/*记录循环次数*/
		func_loop();
		/*循环延时*/
		sleep(5);
	}
	return 0;
}



