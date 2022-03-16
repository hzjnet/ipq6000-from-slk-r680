#ifndef DIALING_H
#define DIALING_H

int flag_dial;
int flag_forcedial;
char value_apn[100];
int mode_rat;
char value_pin[100];
char value_auth[100];
char value_username[100];
char value_password[100];
char type_ip[100];
int flag_ping;
char value_pingaddr[500];
int count_ping;
char mode_usb[100];
char mode_nat[100];
int value_defaultcard;
char value_metric[100];
char value_mtu[100];
char mode_opera[100];

/* 功能：读modem配置文件
** 返回值：void
*/
void readConfig()
{
	sleep(2);
	char tmp_buff[1024];
	char cmd[1024];
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q show modem.%s",name_config);
	memset(tmp_buff,0,sizeof(tmp_buff));
	cmd_recieve_str(cmd,tmp_buff);
	if(strlen(tmp_buff)<0)
	{
		Debug("no Configuration,exit",NULL);
		exit(1);
	}
	//是否开启拨号功能
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.enable",name_config);
	flag_dial = cmd_recieve_int(cmd);
	//是否开启强制拨号功能
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.force_dial",name_config);
	flag_forcedial = cmd_recieve_int(cmd);
	//APN
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.apn",name_config);
	cmd_recieve_str(cmd,value_apn);
	//服务类型
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.smode",name_config);
	mode_rat = cmd_recieve_int(cmd);
	//PIN码
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.pincode",name_config);
	cmd_recieve_str(cmd,value_pin);
	//认证类型
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.auth_type",name_config);
	cmd_recieve_str(cmd,value_auth);
	//用户名
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.username",name_config);
	cmd_recieve_str(cmd,value_username);
	//密码
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.password",name_config);
	cmd_recieve_str(cmd,value_password);
	//拨号IP类型
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.ipv4v6",name_config);
	cmd_recieve_str(cmd,type_ip);
	//是否开启网络监测功能
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.pingen",name_config);
	flag_ping = cmd_recieve_int(cmd);
	//网络监测目标IP
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.pingaddr",name_config);
	cmd_recieve_str(cmd,value_pingaddr);
	//网络监测模式
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.opera_mode",name_config);
	cmd_recieve_str(cmd,mode_opera);
	//网络监测周期
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.count",name_config);
	count_ping = cmd_recieve_int(cmd);
	//拨号上网驱动
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.usbmode",name_config);
	cmd_recieve_str(cmd,mode_usb);
	//拨号上网方式
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.natmode",name_config);
	cmd_recieve_str(cmd,mode_nat);
	
	//默认SIM卡
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.default_card",name_config);
	value_defaultcard = cmd_recieve_int(cmd);
	
	//跃点数
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.metric",name_config);
	cmd_recieve_str(cmd,value_metric);
	
	//MTU
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci -q get modem.%s.mtu",name_config);
	cmd_recieve_str(cmd,value_mtu);
	
	if ( flag_dial == 1 )
		Debug("Dial Enable",NULL);
	if ( flag_forcedial == 1 )
		Debug("Force dial Enable",NULL);
	Debug(value_apn,"APN");
	char tmp_str[10];
	memset(tmp_str,0,sizeof(tmp_str));
	sprintf(tmp_str,"%d",mode_rat);
	Debug(tmp_str,"SMODE");
	Debug(value_auth,"AUTH");
	Debug(value_username,"USERNAME");
	Debug(value_pin,"PIN");
	Debug(type_ip,"IP TYPE");
	memset(tmp_str,0,sizeof(tmp_str));
	sprintf(tmp_str,"%d",value_defaultcard);
	Debug(tmp_str,"Default card slot");
	Debug(value_pingaddr,"IPADDR");
	Debug(mode_opera,"Operating mode");
}

/* 功能：获取模组信息
** 返回结果：0 成功 -1 失败
** 返回值：int
*/
int getModule()
{
	char tmp_buff[4096];
	char value_devicenum[5];
	char cmd[1024];
	while(1){
		//获取ttyUSB*
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"ls -l /sys/bus/usb-serial/devices/ | grep %s/ | awk -F '/' '{print $NF}' | xargs echo -n | sed 's/[[:space:]]//g'",flag_module);
		memset(tmp_buff,0,sizeof(tmp_buff));
		cmd_recieve_str(cmd,tmp_buff);
		check_ttyUSB(tmp_buff);
		//获取接口名称
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"dmesg | grep register | grep %s | tail -n 1 | awk -F ' ' '{print $5}'| awk -F ':' '{print $1}'",flag_module);
		memset(value_ifname,0,sizeof(value_ifname));
		cmd_recieve_str(cmd,value_ifname);
		Debug(value_ifname,"Interface Name");
		//获取所加载模组的型号和版本
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s ATI",path_usb);
		memset(tmp_buff,0,sizeof(tmp_buff));
		cmd_recieve_str(cmd,tmp_buff);
		memset(value_module,0,sizeof(value_module));
		if( strstr(tmp_buff,"1e2d:00b3") )			//西门子模组
			return 0;
		else if( strstr(tmp_buff,"FM650") )		//广和通模组
		{
			//设定模组标志：fm650
			Debug("Modem model is Fibocom FM650",NULL);
			strcpy(value_module,"fm650");
			return 0;
		}
		else if( strstr(tmp_buff,"RM500U") )		//移远5G模组
		{
			//设定模组标志：rm500u
			Debug("Modem model is Quectel RM500U",NULL);
			strcpy(value_module,"rm500u");
			return 0;
		}
		else if( strstr(tmp_buff,"SIM7600") )		//芯讯通4G模组
		{
			//设定模组标志：sim7600
			Debug("Modem model is Simcom SIM7600",NULL);
			strcpy(value_module,"sim7600");
			return 0;
		}
		else if( strstr(tmp_buff,"SIM8200") )		//芯讯通4G模组
		{
			//设定模组标志：sim8200
			Debug("Modem model is Simcom SIM8200",NULL);
			strcpy(value_module,"sim8200");
			memset(value_ifname,0,sizeof(value_ifname));
			strcpy(value_ifname,"wwan0_1");
			Debug(value_ifname,"Interface Name");
			return 0;
		}
		else
			return -1;
	}
}

/* 功能：设置模组拨号方式
** 返回值：void
*/
void setModule()
{
	char tmp_mode_usb[5];
	char tmp_mode_nat[5];
	if(strstr(value_module,"fm650"))		//广和通模组
	{
		//设定拨号上网驱动
		memset(tmp_mode_usb,0,sizeof(tmp_mode_usb));
		if(strstr(mode_usb,"0"))
			strcpy(tmp_mode_usb,"35");	//ECM
		else if(strstr(mode_usb,"1"))
			strcpy(tmp_mode_usb,"39");	//RNDIS
		else if(strstr(mode_usb,"2"))
			strcpy(tmp_mode_usb,"36");	//NCM
		else
			strcpy(tmp_mode_usb,"36");
		//设定拨号上网方式
		memset(tmp_mode_nat,0,sizeof(tmp_mode_nat));
		if(strstr(mode_nat,"0"))
			strcpy(tmp_mode_nat,"1");	//网卡模式
		else if(strstr(mode_nat,"1"))
			strcpy(tmp_mode_nat,"0");	//路由模式
		else
			strcpy(tmp_mode_nat,"1");
		//设定fm650模组拨号方式
		setModule_FM650(tmp_mode_usb,tmp_mode_nat);
	}
	else if(strstr(value_module,"rm500u"))	//移远模组
	{
		//设定拨号上网驱动
		memset(tmp_mode_usb,0,sizeof(tmp_mode_usb));
		if(strstr(mode_usb,"0"))
			strcpy(tmp_mode_usb,"1");	//ECM
		else if(strstr(mode_usb,"1"))
			strcpy(tmp_mode_usb,"3");	//RNDIS
		else if(strstr(mode_usb,"2"))
			strcpy(tmp_mode_usb,"5");	//NCM
		else
			strcpy(tmp_mode_usb,"5");
		//设定拨号上网方式
		memset(tmp_mode_nat,0,sizeof(tmp_mode_nat));
		if(strstr(mode_nat,"0"))
			strcpy(tmp_mode_nat,"0");	//网卡模式
		else if(strstr(mode_nat,"1"))
			strcpy(tmp_mode_nat,"1");	//路由模式
		else
			strcpy(tmp_mode_nat,"0");
		setModule_RM500U(tmp_mode_usb,tmp_mode_nat);
	}
	else if(strstr(value_module,"sim8200"))
	{
		setModule_SIM8200();
	}
}
/* 功能：读取驻网信息
** 返回值：void
*/
void getModem()
{
	//打印模组驻网信息到/tmp/tmp_modem文件中
	char cmd[1024];
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"echo \"######%s######\" > %s",value_module,path_outfile);
	cmd_recieve_str(cmd,NULL);
	//获取IMEI
	getIMEI();
	//获取读卡状态
	getCPIN();
	//获取IMSI
	getIMSI();
	//获取运营商信息
	getCOPS();
	//获取信号强度
	getCSQ();
	//解析模组所处网络小区信息
	if(strstr(value_module,"mv31w"))			//西门子模组
		return;
	else if(strstr(value_module,"sim7600"))		//芯讯通4G模组
		analyticCPSI_SIM7600();
	else if(strstr(value_module,"sim8200"))		//芯讯通5G模组
		analyticCPSI_SIM8200();
	else if(strstr(value_module,"fm650"))		//广和通模组
		analyticINFO_FM650();
	else if(strstr(value_module,"rm500u"))		//移远模组
		analyticQENG_RM500U();
	//将/tmp/tmp_mdoem文件复制到/tmp/modem文件中
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"cat %s > %s",path_outfile,path_formfile);
	system(cmd);
	//cmd_recieve_str(cmd,NULL);
}

/* 功能：获取IMEI
** 返回值：void
*/
void getIMEI()
{
	char tmp_buff[1024];
	char cmd[1024];
	int count=0;
	while(1)
	{
		//获取IMEI，最多10次
		memset(cmd,0,sizeof(cmd));
		memset(tmp_buff,0,sizeof(tmp_buff));
		sprintf(cmd,"sendat %s \"AT+CGSN\" 500 | sed -n '2p'",path_usb);
		cmd_recieve_str(cmd,tmp_buff); 
		//printf("IMEI:%s\n",tmp_buff);
		//判断结果是否为空或符合IMEI标准
		if(tmp_buff != NULL && isInt(tmp_buff)==0)
		{
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"echo \"+IMEI: %s\" >> %s",tmp_buff,path_outfile);
			cmd_recieve_str(cmd,NULL);
			Debug(tmp_buff,"+IMEI");
			break;
		}
		if(count==9)
			break;
		count++;
	}
}
/* 功能：从/tmp/tmp_modem文件中移除上一次读卡记录
** 返回值：void
*/
void removeCPIN()
{
	char cmd[1024];
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sed -i '/+CPIN:/d' %s",path_outfile);
	cmd_recieve_str(cmd,NULL);
}
/* 功能：切换飞行模式
** 返回值：void
*/
void setFlightmode()
{
	char cmd[1024];
	//切换到飞行模式
	memset(cmd,0,sizeof(cmd));
	if(strstr(value_module,"fm650"))	//广和通模组
	{
		sprintf(cmd,"sendat %s \"AT+CFUN=4\" > /dev/null 2>&1",path_usb);
		cmd_recieve_str(cmd,NULL);
	}else{								//其他模组
		sprintf(cmd,"sendat %s \"AT+CFUN=0\" > /dev/null 2>&1",path_usb);
		cmd_recieve_str(cmd,NULL);
	}
	//切换到正常通讯模式
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s \"AT+CFUN=1\" > /dev/null 2>&1",path_usb);
	cmd_recieve_str(cmd,NULL);
	cmd_recieve_str(cmd,NULL);
	sleep(3);
}
/* 功能：获取读卡状态
** 返回值：void
*/
void getCPIN()
{
	char tmp_buff[1024];
	char cmd[1024];
	int count=0;
	while(1)
	{
		memset(cmd,0,sizeof(cmd));
		memset(tmp_buff,0,sizeof(tmp_buff));
		sprintf(cmd,"sendat %s 'AT+CPIN?' | grep \"READY\"",path_usb);
		cmd_recieve_str(cmd,tmp_buff);
		if(strstr(tmp_buff,"READY"))	//读卡，打印结果到/tmp/tmp_mdoem文件，跳出循环
		{
			removeCPIN();
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"echo \"%s\" >> %s",tmp_buff,path_outfile);
			cmd_recieve_str(cmd,NULL);
			Debug(tmp_buff,NULL);
			break;
		}else{							
			if(count==0)		//第1次不读卡，打印结果到/tmp/tmp_mdoem文件
			{
				removeCPIN();
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"echo \"+CPIN: SIM card not inserted\" >> %s",path_outfile);
				cmd_recieve_str(cmd,NULL);
				Debug("SIM card not inserted",NULL);
				cmd_recieve_str(cmd_ifdown,NULL);
				//将/tmp/tmp_mdoem文件复制到/tmp/modem文件中
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"cat %s > %s",path_outfile,path_formfile);
				cmd_recieve_str(cmd,NULL);
				//信号灯控制
				controlLED();
			}
			else if(count==4) 	//第5次不读卡，切换SIM卡
			{
				system("killall simcom-cm");
				//修改当前读卡位置
				if(strstr(value_module,"fm650"))
					SwitchSIM_FM650(false);
				else if(strstr(value_module,"sim8200"))
					SwitchSIM_SIM8200(false);
			}
			else if(count==9) 	//第10次不读卡，切换为[优先卡槽]并飞行模式
			{
				system("killall simcom-cm");
				//设置[当前卡槽]为[优先卡槽]
				if(strstr(value_module,"fm650"))
					SwitchSIM_SIM8200(true);
				else if(strstr(value_module,"sim8200"))
					SwitchSIM_SIM8200(true);
				setFlightmode();
				count=-1;
			}
			sleep(2);
		}
		count++;
	}
}
/* 功能：获取运营商信息
** 返回值：void
*/
void getCOPS()
{
	char tmp_buff[1024];
	char cmd[1024];
	int count=0;
	while(1)
	{
		//循环获取运营商信息，最多2次
		memset(cmd,0,sizeof(cmd));
		memset(tmp_buff,0,sizeof(tmp_buff));
		sprintf(cmd,"sendat %s \"AT+COPS?\" | grep \"+COPS:\"",path_usb);
		cmd_recieve_str(cmd,tmp_buff);
		if(strstr(tmp_buff,"+COPS:"))
		{
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"echo \"%s\" >> %s",tmp_buff,path_outfile);
			cmd_recieve_str(cmd,NULL);
			Debug(tmp_buff,NULL);
			break;
		}
		
		if(count==1)
			break;
		count++;
	}
}
/* 功能：获取IMSI
** 返回值：void
*/
void getIMSI()
{
	char tmp_buff[1024];
	char cmd[1024];
	int count=0;
	while(1)
	{
		//获取IMSI，最多2次
		memset(cmd,0,sizeof(cmd));
		memset(tmp_buff,0,sizeof(tmp_buff));
		sprintf(cmd,"sendat %s \"AT+CIMI\" 500 | sed -n '2p'",path_usb);
		cmd_recieve_str(cmd,tmp_buff);
		//判断结果是否为空或符合IMSI标准
		if(tmp_buff != NULL && isInt(tmp_buff)==0)
		{
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"echo \"+IMSI: %s\" >> %s",tmp_buff,path_outfile);
			cmd_recieve_str(cmd,NULL);
			Debug(tmp_buff,"+IMSI");
			break;
		}
		
		if(count==1)
			break;
		count++;
	}
}
/* 功能：获取信号强度
** 返回值：void
*/
void getCSQ()
{
	char tmp_buff[1024];
	char cmd[1024];
	memset(cmd,0,sizeof(cmd));
	memset(tmp_buff,0,sizeof(tmp_buff));
	sprintf(cmd,"sendat %s \"AT+CSQ\" | grep \"+CSQ:\"",path_usb);
	cmd_recieve_str(cmd,tmp_buff);
	if(strstr(tmp_buff,"+CSQ:"))
	{
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"echo \"%s\" >> %s",tmp_buff,path_outfile);
		cmd_recieve_str(cmd,NULL);
		Debug(tmp_buff,NULL);
	}
}
/* 功能：解析RSRP，转换为信号值
** 返回值：void
*/
void analyticRSRP(int tmp_csq)
{
	
	if(tmp_csq < -140 )					//<-140			无信号
		value_signal=0;
	else if(tmp_csq < -125 )			//-126~-140		1格
		value_signal=1;
	else if(tmp_csq < -115 )			//-116~-125		2格
		value_signal=2;
	else if(tmp_csq < -105 )			//-106~-115 	3格
		value_signal=3;
	else if(tmp_csq < -95 )				//-96~-105		4格
		value_signal=4;
	else if(tmp_csq < -10 )				//-11~-95 		5格
		value_signal=5;
	char cmd[1024];
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"echo \"+SVAL: %d\" >> %s",value_signal,path_outfile);
	cmd_recieve_str(cmd,NULL);
	char tmp_buff[1024];	
	memset(tmp_buff,0,sizeof(tmp_buff));
	sprintf(tmp_buff,"RSRP: %d SVAL: %d",tmp_csq,value_signal);
	Debug(tmp_buff,NULL);
}

/* 功能：解析CSQ，转换为信号值
** 返回值：void
*/
void analyticCSQ()
{
	char tmp_buff[1024];
	char cmd[1024];
	memset(cmd,0,sizeof(cmd));
	memset(tmp_buff,0,sizeof(tmp_buff));
	sprintf(cmd,"sendat %s \"AT+CSQ\" | grep \"+CSQ:\"",path_usb);
	cmd_recieve_str(cmd,tmp_buff);
	if(strstr(tmp_buff,"+CSQ:"))
	{
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"echo \"%s\" | awk '{print $2}' | awk -F ',' '{print $1}'",tmp_buff);
		int tmp_csq;
		tmp_csq=cmd_recieve_int(cmd);
		if(tmp_csq == 99 ) 			//99,99		无信号
			value_signal=0;
		else if(tmp_csq < 7 )		//0-6,99	无信号
			value_signal=0;
		else if(tmp_csq < 15 )		//7-14,99	1格
			value_signal=1;
		else if(tmp_csq < 18 )		//15-17,99	2格
			value_signal=2;
		else if(tmp_csq < 21 )		//18-20,99	3格
			value_signal=3;
		else if(tmp_csq < 25 )		//21-24,99	4格
			value_signal=4;
		else if(tmp_csq < 31 )		//25-31,99	5格
			value_signal=5;
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"echo \"+SVAL: %d\" >> %s",value_signal,path_outfile);
		cmd_recieve_str(cmd,NULL);
		
		memset(tmp_buff,0,sizeof(tmp_buff));
		sprintf(tmp_buff,"RSSI: %d SVAL: %d",tmp_csq,value_signal);
		Debug(tmp_buff,NULL);
	}
}
/* 功能：设定PIN码
** 返回值：void
*/
void setPINCode()
{
	char cmd[1024];
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s AT+CPIN=%s",path_usb,value_pin);
	char tmp_buff[1024];
	memset(tmp_buff,0,sizeof(tmp_buff));
	cmd_recieve_str(cmd,tmp_buff);
	if(strstr(tmp_buff,"OK"))
		Debug(value_pin,"Set PIN Code");
	else if(strstr(tmp_buff,"ERROR"))
		Debug(value_pin,"Failed-1 to set PIN Code");
	else
		Debug(value_pin,"Failed-2 to set PIN Code");
}
/* 功能：设定拨号信息：APN、服务类型和*锁频
** 返回值：void
*/
void setModem()
{
	if(count_loop!=0)	//仅设置一次
		return;
	char cmd[1024];
	char tmp_buff[1024];
	//设定PIN码
	if(strlen(value_pin)>0)
	{
		setPINCode();
	}
	//设定APN、IP类型、用户名、密码和认证方式
	if(strstr(value_module,"rm500u"))														//移远模组
	{
		int tmp_type_ip;
		if(strstr(type_ip,"IPV4"))
			tmp_type_ip=1;
		else if(strstr(type_ip,"IPV6"))
			tmp_type_ip=2;
		else if(strstr(type_ip,"IPV4V6"))
			tmp_type_ip=3;
		else
			tmp_type_ip=3;
_SETAPN_AGAIN1:
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s 'AT+QICSGP=1,%d,\"%s\",\"%s\",\"%s\",%s'",path_usb,tmp_type_ip,value_apn,value_username,value_password,value_auth);
		memset(tmp_buff,0,sizeof(tmp_buff));
		cmd_recieve_str(cmd,tmp_buff);
		if(strstr(tmp_buff,"OK")) 			//获取设定结果，OK
			{
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"Set APN:%s IP TYPE:%s USERNAME:%s PASSWORD:%s AUTH:%s",value_apn,type_ip,value_username,value_password,value_auth);
				Debug(cmd,NULL);
			}
			else if(strstr(tmp_buff,"ERROR"))	//获取设定结果，ERROR
			{
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"Failed-1 to set APN:%s IP TYPE:%s USERNAME:%s PASSWORD:%s AUTH:%s",value_apn,type_ip,value_username,value_password,value_auth);
				Debug(cmd,NULL);
			}
			else								//未获取设定结果，通过goto再次设定
			{
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"Failed-2 to set APN:%s IP TYPE:%s USERNAME:%s PASSWORD:%s AUTH:%s",value_apn,type_ip,value_username,value_password,value_auth);
				Debug(cmd,NULL);
				goto _SETAPN_AGAIN1;
			}
	}
	else{
		/* memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s \"AT+CGDCONT=1,\"IP\"\" > /dev/null 2>&1",path_usb);
		memset(tmp_buff,0,sizeof(tmp_buff));
		cmd_recieve_str(cmd,tmp_buff);
		if(strcmp(tmp_buff,"OK")>=0)
			Debug("Set no APN","");
		else if(strcmp(tmp_buff,"ERROR")>=0)
			Debug("Failed:-1 to set no APN","");
		else
			Debug("Failed:-2 to set no APN",""); */
	//设定APN和拨号IP类型
_SETAPN_AGAIN2:
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"sendat %s 'AT+CGDCONT=1,\"%s\",\"%s\"'",path_usb,type_ip,value_apn);
			memset(tmp_buff,0,sizeof(tmp_buff));
			cmd_recieve_str(cmd,tmp_buff);
			if(strstr(tmp_buff,"OK")) 			//获取设定结果，OK
			{
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"Set APN:%s IP TYPE:%s",value_apn,type_ip);
				Debug(cmd,NULL);
			}
			else if(strstr(tmp_buff,"ERROR"))	//获取设定结果，ERROR
			{
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"Failed-1 to set APN:%s IP TYPE:%s",value_apn,type_ip);
				Debug(cmd,NULL);
			}
			else								//未获取设定结果，通过goto再次设定
			{
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"Failed-2 to set APN:%s IP TYPE:%s",value_apn,type_ip);
				Debug(cmd,NULL);
				goto _SETAPN_AGAIN2;
			}
			
			if(strstr(value_module,"mv31w"))											//西门子模组
			{
				return;
			}
			else if(strstr(value_module,"fm650") && strlen(value_username) > 0)		//广和通模组
			{
				//设定APN下的用户名、密码和认证方式
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"sendat %s 'AT+MGAUTH=1,%s,\"%s\",\"%s\"'",path_usb,value_auth,value_username,value_password);
				memset(tmp_buff,0,sizeof(tmp_buff));
				cmd_recieve_str(cmd,tmp_buff);
				if(strstr(tmp_buff,"OK"))
				{
					memset(cmd,0,sizeof(cmd));
					sprintf(cmd,"Set Auth:%s USERNAME:%s PASSWORD:%s",value_auth,value_username,value_password);
					Debug(cmd,NULL);
				}
				else if(strstr(tmp_buff,"ERROR"))
				{
					memset(cmd,0,sizeof(cmd));
					sprintf(cmd,"Failed-1 to set Auth:%s USERNAME:%s PASSWORD:%s",value_auth,value_username,value_password);
					Debug(cmd,NULL);
				}
				else
				{
					memset(cmd,0,sizeof(cmd));
					sprintf(cmd,"Failed-2 to set Auth:%s USERNAME:%s PASSWORD:%s",value_auth,value_username,value_password);
					Debug(cmd,NULL);
				}
			}
		//}
		/* else{
			//Debug("set apn","");
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"sendat %s 'AT+CGDCONT=1,\"%s\"'",path_usb,type_ip);
			memset(tmp_buff,0,sizeof(tmp_buff));
			Debug(cmd,"cmd:");
			cmd_recieve_str(cmd,tmp_buff);
			printf("%s\n",tmp_buff);
			//printf("%d\n",strstr(tmp_buff, "OK"));
			if(strstr(tmp_buff,"OK"))
				Debug(type_ip,"Set no APN,IP type");
			else if(strstr(tmp_buff,"ERROR"))
				Debug(type_ip,"Failed-1 to set no APN,IP type");
			else
				Debug(type_ip,"Failed-2 to set no APN,IP type");
		}	 */
	}
	//设定服务类型
	if(strstr(value_module,"mv31w"))			//西门子模组
		return;
	else if(strstr(value_module,"sim7600"))		//芯讯通4G模组
	{
		int tmp_mode_rat;
		if(mode_rat==0)							//0：2 自动
			tmp_mode_rat=2;
		else if(mode_rat==1)					//1: 14	仅3G
			tmp_mode_rat=14;
		else if(mode_rat==2)					//2: 38	仅4G
			tmp_mode_rat=38;
		else if(mode_rat==3)					//3：54	4G/3G
			tmp_mode_rat=54;
		else if(mode_rat==5)					//5: 13	仅2G
			tmp_mode_rat=13;
		else if(mode_rat==8)					//8: 19	2G/3G
			tmp_mode_rat=19;
		else if(mode_rat==9)					//9: 51 2G/4G
			tmp_mode_rat=51;
		else if(mode_rat==10)					//10: 39 2G/3G/4G
			tmp_mode_rat=39;
		else
			tmp_mode_rat=2;
		//设定sim7600模组上网服务类型
		setRATMODE_SIM7600(tmp_mode_rat);
	}
	else if(strstr(value_module,"sim8200"))		//芯讯通5G模组
	{
		int tmp_mode_rat;
		if(mode_rat==0)							//0：2   自动
			tmp_mode_rat=2;
		else if(mode_rat==1)					//1: 14	 仅3G
			tmp_mode_rat=14;
		else if(mode_rat==2)					//2: 38	 仅4G
			tmp_mode_rat=38;
		else if(mode_rat==3)					//3：54	 4G/3G
			tmp_mode_rat=54;
		else if(mode_rat==4)					//4: 71	 仅5G
			tmp_mode_rat=71;
		else if(mode_rat==6)					//6: 109 5G/4G
			tmp_mode_rat=109;
		else if(mode_rat==7)					//7: 55 5G/4G/3G
			tmp_mode_rat=55;
		else
			tmp_mode_rat=2;
		//设定sim8200模组上网服务类型
		setRATMODE_SIM8200(tmp_mode_rat);
	}
	else if(strstr(value_module,"fm650"))		//广和通模组
	{
		int tmp_mode_rat;
		if(mode_rat==0)							//0：20 自动
			tmp_mode_rat=20;
		else if(mode_rat==1)					//1: 2	仅3G
			tmp_mode_rat=2;
		else if(mode_rat==2)					//2: 3	仅4G
			tmp_mode_rat=3;
		else if(mode_rat==3)					//3：4	4G/3G
			tmp_mode_rat=4;
		else if(mode_rat==4)					//4: 14	仅5G
			tmp_mode_rat=14;
		else if(mode_rat==6)					//6: 16	5G/4G
			tmp_mode_rat=16;
		else if(mode_rat==7)					//7: 20 5G/4G/3G
			tmp_mode_rat=20;
		else
			tmp_mode_rat=20;
		//设定fm650模组上网服务类型
		setRATMODE_FM650(tmp_mode_rat);
	}
	else if(strstr(value_module,"rm500u"))
	{
		char tmp_mode_rat[100];
		memset(tmp_mode_rat,0,sizeof(tmp_mode_rat));
		if(mode_rat==0)							//0：20 自动
			strcpy(tmp_mode_rat,"AUTO");
		else if(mode_rat==1)					//1: 2	仅3G
			strcpy(tmp_mode_rat,"WCDMA");
		else if(mode_rat==2)					//2: 3	仅4G
			strcpy(tmp_mode_rat,"LTE");
		else if(mode_rat==3)					//3：4	4G/3G
			strcpy(tmp_mode_rat,"LTE:WCDMA");
		else if(mode_rat==4)					//4: 14	仅5G
			strcpy(tmp_mode_rat,"NR5G");
		else if(mode_rat==6)					//6: 16	5G/4G
			strcpy(tmp_mode_rat,"NR5G:LTE");
		else if(mode_rat==7)					//7: 20 5G/4G/3G
			strcpy(tmp_mode_rat,"NR5G:LTE:WCDMA");
		else
			strcpy(tmp_mode_rat,"AUTO");
		setRATMODE_R500U(tmp_mode_rat);
	}	
}
/* 功能：拨号
** 返回值：void
*/
void Dial()
{
	//开启拨号功能
	if(flag_dial==1)
	{
		//信号格大于2或者开启强制拨号时进行拨号
		if(value_signal>=2 || flag_forcedial == 1)
		{
			char cmd[1024];
			if(strstr(value_module,"mv31w"))			//西门子模组
				return;
			else if(strstr(value_module,"sim7600"))		//芯讯通4G模组
			{
				char value_ip_ifconfig[500];
				memset(value_ip_ifconfig,0,sizeof(value_ip_ifconfig));
				//获取模组拨号状态
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"sendat %s 'AT$QCRMCALL?' 1000 | grep \"$QCRMCALL:\" | grep \"V4\" | wc -l",path_usb);
				int result=cmd_recieve_int(cmd);
				//未拨号
				if(result == 0)
				{
					//对sim7600模组进行拨号
					Debug("Dial SIM7600",NULL);
					Dial_SIM7600();
				}
				//获取网卡IP
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"ifconfig %s | grep \"inet addr\" | awk -F ':' '{print$2}' | awk -F ' ' '{print$1}'",value_ifname);
				cmd_recieve_str(cmd,value_ip_ifconfig);
				//不存在网卡IP时，重启网卡
				if(strlen(value_ip_ifconfig)==0)
				{
					Debug(cmd_ifup,NULL);
					cmd_recieve_str(cmd_ifup,NULL);
				}
			}
			else if(strstr(value_module,"sim8200"))		//芯讯通5G模组
			{
				char value_ip_ifconfig[500];
				memset(value_ip_ifconfig,0,sizeof(value_ip_ifconfig));
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"ifconfig %s | grep \"inet addr\" | awk -F ':' '{print$2}' | awk -F ' ' '{print$1}'",value_ifname);
				//Debug(cmd,NULL);
				cmd_recieve_str(cmd,value_ip_ifconfig);
				//不存在网卡IP时，尝试拨号
				if(value_ip_ifconfig == NULL || strlen(value_ip_ifconfig)==0)
				{
					Debug("Dial SIM8200",NULL);
					system("killall simcom-cm");
					sleep(3);
					Dial_SIM8200();
					sleep(5);
					Debug(cmd_ifup,NULL);
					cmd_recieve_str(cmd_ifup,NULL);
				}
			}
			else if(strstr(value_module,"fm650"))		//广和通模组
			{
				char value_ip_dial[500];
				char value_ip_ifconfig[500];
				memset(value_ip_dial,0,sizeof(value_ip_dial));
				memset(value_ip_ifconfig,0,sizeof(value_ip_ifconfig));
				//获取模组拨号IP
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"sendat %s AT+GTRNDIS? | grep \"+GTRNDIS: 1\" | awk -F ',' '{print$3}'| awk -F '\"' '{print$2}'",path_usb);
				cmd_recieve_str(cmd,value_ip_dial);
				//不存在拨号IP，尝试拨号
				if(strlen(value_ip_dial)==0)
				{
					//对fm650模组进行拨号
					Debug("Dial FM650",NULL);
					Dial_FM650();
				}
				//获取网卡IP
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"ifconfig %s | grep \"inet addr\" | awk -F ':' '{print$2}' | awk -F ' ' '{print$1}'",value_ifname);
				cmd_recieve_str(cmd,value_ip_ifconfig);
				//不存在网卡IP或模组IP和网卡IP不一致时，重启网卡
				if(strlen(value_ip_ifconfig)==0 || !strstr(value_ip_dial,value_ip_ifconfig))
				{
					Debug(cmd_ifup,NULL);
					cmd_recieve_str(cmd_ifup,NULL);
				}
			}
			else if(strstr(value_module,"rm500u"))		//移远模组
			{
				char value_ip_dial[500];
				char value_ip_ifconfig[500];
				memset(value_ip_dial,0,sizeof(value_ip_dial));
				memset(value_ip_ifconfig,0,sizeof(value_ip_ifconfig));
				//获取模组拨号IP
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"sendat %s AT+QNETDEVSTATUS=1 | grep \"+QNETDEVSTATUS:\" | awk -F ',' '{print$1}'| awk -F ' ' '{print$2}'",path_usb);
				cmd_recieve_str(cmd,value_ip_dial);
				//不存在拨号IP，尝试拨号
				if(strlen(value_ip_dial)==0)
				{
					//对fm650模组进行拨号
					Debug("Dial R500U",NULL);
					Dial_R500U();
				}
				//获取网卡IP
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"ifconfig %s | grep \"inet addr\" | awk -F ':' '{print$2}' | awk -F ' ' '{print$1}'",value_ifname);
				cmd_recieve_str(cmd,value_ip_ifconfig);
				//不存在网卡IP或模组IP和网卡IP不一致时，重启网卡
				if(strlen(value_ip_ifconfig)==0 || !strstr(value_ip_dial,value_ip_ifconfig))
				{
					Debug(cmd_ifup,NULL);
					cmd_recieve_str(cmd_ifup,NULL);
				}
			}
		}
	}
}
/* 功能：网络监测
** 返回值：void
*/
void checkDNS()
{
	//开启拨号功能和PING功能时，进行网络监测
	if(flag_dial==1 && flag_ping==1)
	{
		//获取ping状态
		int result=ping_status(value_pingaddr,value_ifname,count_ping);
		if(result==0) //网络正常
		{
			Debug("Network is OK",NULL);
		}else	//网络异常
		{
			Debug("Network is failed",NULL);
			if(strstr(mode_opera,"reboot_route"))		//重启设备
			{
				system("reboot");
			}
			else if(strstr(mode_opera,"switch_card"))	//切换SIM卡
			{
				if(strstr(value_module,"fm650"))
				{
					SwitchSIM_FM650(false);
				}
				else if(strstr(value_module,"sim8200"))
				{
					SwitchSIM_SIM8200(false);
				}
			}
			else if(strstr(mode_opera,"airplane_mode"))	//飞行模式
			{
				Debug("Set Flight mode",NULL);
				setFlightmode();
			}
		}
	}
}
/* 功能：控制灯（RSSI模式）
** 传参：count 系统中rssi灯的数量
** 返回值：void
*/
void controlLED_RSSI(int count)
{
	char cmd[1024];
	char tmp_buff[1024];
	memset(tmp_buff,0,sizeof(tmp_buff));
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"cat %s |grep \"+SVAL:\" |awk '{print $2}'",path_formfile);
	int signal=cmd_recieve_int(cmd);
	int i;
	int signal_array[4]={1,2,4,5};
	for(i=1;i<=count;i++){
		if(signal>=signal_array[i-1]){
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"echo 1 > $(ls /sys/class/leds/*rssi%d/brightness)",i);
			cmd_recieve_str(cmd,NULL);
		}else{
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"echo 0 > $(ls /sys/class/leds/*rssi%d/brightness)",i);
			cmd_recieve_str(cmd,NULL);
		}
	}
}
/* 功能：控制灯
** 返回值：void
*/
void controlLED()
{
	char cmd[1024];
	char type_led[1024];
	//读取所有灯的名称
	memset(cmd,0,sizeof(cmd));
	strcpy(cmd,"ls /sys/class/leds | xargs echo -n | sed 's/^[[:space:]]*//g'");
	memset(type_led,0,sizeof(type_led));
	cmd_recieve_str(cmd,type_led);
	if(strstr(type_led,"rssi"))				//rssi
	{
		controlLED_RSSI(countstr(type_led,"rssi")); //获取系统中rssi灯的数量并控制rssi灯
	}else if(strstr(type_led,"rsrp"))		//rsrp
		return;
	
}
#endif