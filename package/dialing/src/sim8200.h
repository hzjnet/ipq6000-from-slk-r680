#include "dialing.h"
/* 功能：设定sim8200模组拨号方式
** 返回值：void
*/
void setModule_SIM8200(char *tmp_mode_usb,char *tmp_mode_nat)
{
	//终止simcom-cm程序
	system("killall simcom-cm");
	char cmd[1024];
	int tmp_int_buff;
	char tmp_str_buff[1024];
	//确认模组正常工作
	while(1)
	{
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s 'AT' | grep OK |wc -l",path_usb);
		tmp_int_buff = cmd_recieve_int(cmd);
		if (tmp_int_buff == 1)
			break;
	}
	
	//设定当前读卡位置
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s 'AT+SMSIMCFG?' | grep \"+SMSIMCFG: 1,%d\" | awk -F ',' '{print $3}'",path_usb,(value_defaultcard+1));
	int result = cmd_recieve_int(cmd);
	//printf("Current card slot:%d\n",result);
	if(result != 2) //[优先卡槽] == [当前卡槽]
	{
		memset(tmp_str_buff,0,sizeof(tmp_str_buff));
		sprintf(tmp_str_buff,"%d",(value_defaultcard+1));
		Debug(tmp_str_buff,"AT+SMSIMCFG");
	}else
	{
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s 'AT+SMSIMCFG=1,%d'",path_usb,(value_defaultcard+1));
		cmd_recieve_str(cmd,NULL);
		memset(tmp_str_buff,0,sizeof(tmp_str_buff));
		sprintf(tmp_str_buff,"%d",(value_defaultcard+1));
		Debug(tmp_str_buff,"Set AT+SMSIMCFG");
		sleep(2);
	}
}

/* 功能：解析sim8200所处网络小区信息
** 返回值：void
*/
void analyticCPSI_SIM8200()
{
	char tmp_buff[2048];
	char cmd[1024];
	char nservice[100];
	char band[100];
	memset(cmd,0,sizeof(cmd));
	memset(tmp_buff,0,sizeof(tmp_buff));
	memset(nservice,0,sizeof(nservice));
	memset(band,0,sizeof(band));
	sprintf(cmd,"sendat %s \"AT+CPSI?\" 500 > %s",path_usb,path_at);
	cmd_recieve_str(cmd,NULL);
	memset(tmp_buff,0,sizeof(tmp_buff));
	sprintf(cmd,"cat %s | grep \"+CPSI:\" | awk -F ': ' '{print $2}'",path_at);
	cmd_recieve_str(cmd,tmp_buff);
	if(strstr(tmp_buff,"NO SERVICE") || strstr(tmp_buff,"32768"))
	{
		Debug("NO SERVICE",NULL);
		return;
	}
	else if(strstr(tmp_buff,"Online"))
	{
		int count=countstr(tmp_buff,",");
		char array_CPSI[count][50];
		if(separate_string2array(tmp_buff,",",count,50,(char *)&array_CPSI) != count)
		{
			Debug("Format CPSI error!",NULL);
		}
		//解析服务类型
		if(strstr(tmp_buff,"NR"))
		{
			strcat(nservice,"NR");
			//解析上网频段
			strcat(band,array_CPSI[6]);
			int tmp_rsrp=atoi(array_CPSI[9])/10;
			analyticRSRP(tmp_rsrp);
		}
		else if(strstr(tmp_buff,"LTE"))
		{
			strcat(nservice,"LTE");
			//解析上网频段
			strcat(band,array_CPSI[6]);
			int tmp_rsrp=atoi(array_CPSI[11])/10;
			analyticRSRP(tmp_rsrp);
		}else if(strstr(tmp_buff,"CDMA"))
		{
			strcat(nservice,"WCDMA");
			//解析上网频段
			strcat(band,array_CPSI[6]);
			analyticCSQ();
		}else if(strstr(tmp_buff,"GSM"))
		{
			strcat(nservice,"GSM");
			//解析上网频段
			strcat(band,array_CPSI[6]);
			analyticCSQ();
		}else{
			strcat(nservice,array_CPSI[0]);
			//解析上网频段
			strcat(band,array_CPSI[6]);
			analyticCSQ();
		}	
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"%s BAND=%s",nservice,band);
		Debug(cmd,NULL);
		
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"echo \"+CPSI: %s\" >> %s",nservice,path_outfile);
		cmd_recieve_str(cmd,NULL);
		
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"echo \"+BAND: %s\" >> %s",band,path_outfile);
		cmd_recieve_str(cmd,NULL);
	}
}
/* 功能：设定sim8200模组上网服务类型
** 返回值：void
*/
void setRATMODE_SIM8200(int mode)
{

	char cmd[1024];
	char tmp_buff[1024];
	//设定服务类型
_SETRAT_SIM8200_AGAIN:	
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s \"AT+CNMP=%d\"",path_usb,mode);
	memset(tmp_buff,0,sizeof(tmp_buff));
	cmd_recieve_str(cmd,tmp_buff);
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"%d",mode);
	if(strstr(tmp_buff,"OK"))				//获取设定结果，OK
		Debug(cmd,"Set AT+CNMP");
	else if(strstr(tmp_buff,"ERROR"))		//获取设定结果，ERROR
		Debug(cmd,"Failed-1 to set AT+CNMP");
	else{									//获取设定结果失败，通过goto再次设定
		Debug(cmd,"Failed-2 to set AT+CNMP");
		goto _SETRAT_SIM8200_AGAIN;
	}
}
/* 功能：对sim8200模组进行拨号
** 返回值：void
*/
void Dial_SIM8200()
{
	int count=0;
	char cmd[1024];
	char tmp_buff[1024];
	//设置modem网卡信息：协议、网卡名和跃点数
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci set network.%s.proto='dhcp'",name_config);
	cmd_recieve_str(cmd,NULL);
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"uci set network.%s.ifname='%s'",name_config,value_ifname);
	cmd_recieve_str(cmd,NULL);
	memset(cmd,0,sizeof(cmd));
	if(value_metric != NULL && strlen(value_metric)>0)
	{
		sprintf(cmd,"uci set network.%s.metric='%s'",name_config,value_metric);
		cmd_recieve_str(cmd,NULL);
	}else{
		sprintf(cmd,"uci set network.%s.metric='50'",name_config);
		cmd_recieve_str(cmd,NULL);
	}
	if(value_mtu != NULL && strlen(value_mtu)>0)
	{
		sprintf(cmd,"uci set network.%s.mtu='%s'",name_config,value_mtu);
		cmd_recieve_str(cmd,NULL);
	}else{
		sprintf(cmd,"uci set network.%s.mtu='1400'",name_config);
		cmd_recieve_str(cmd,NULL);
	}
	memset(cmd,0,sizeof(cmd));
	strcpy(cmd,"uci commit network");
	cmd_recieve_str(cmd,NULL);
	char tmp_type_ip[20];
	memset(tmp_type_ip,0,sizeof(tmp_type_ip));
	sprintf(tmp_type_ip,"");
	if (strstr(type_ip,"IPV4"))
		sprintf(tmp_type_ip,"");
	if(strstr(type_ip,"IPV6"))
		sprintf(tmp_type_ip,"-6");
	if(strstr(type_ip,"IPV4V6"))
		sprintf(tmp_type_ip,"-4 -6");
	Debug(type_ip,NULL);
	Debug(tmp_type_ip,NULL);
	if(value_apn != NULL && strlen(value_apn) > 0)
	{
		if(value_username != NULL && strlen(value_username) > 0)
		{
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"simcom-cm -s %s %s %s %s %s &",value_apn,value_username,value_password,value_auth,tmp_type_ip);
			system(cmd);
			Debug(cmd,"Dial SIM8200 OK");
		}else{
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"simcom-cm -s %s %s &",value_apn,tmp_type_ip);
			system(cmd);
			Debug(cmd,"Dial SIM8200 OK");
		}
	}else{
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"simcom-cm %s &",tmp_type_ip);
		Debug(cmd,NULL);
		system(cmd);
		Debug("Dial SIM8200 OK",NULL);
	}
	sleep(5);
}

/* 功能：双卡模式切换SIM卡
** 返回值：void
*/
void SwitchSIM_SIM8200(bool isdefault)
{
	char cmd[1024];
	char tmp_buff[1024];
	int value_nextcard=value_defaultcard;
	if(isdefault) //设置默认卡槽
	{
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s 'AT+SMSIMCFG=1,%d'",path_usb,(value_defaultcard+1));
		cmd_recieve_str(cmd,NULL);
		memset(tmp_buff,0,sizeof(tmp_buff));
		sprintf(tmp_buff,"%d",(value_defaultcard+1));
		Debug(tmp_buff,"Set AT+SMSIMCFG");
		sleep(2);
	}
	else		//获取未激活卡槽并激活
	{
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s 'AT+SMSIMCFG?' | grep \"+SMSIMCFG: 1,1\" | awk -F ',' '{print $3}'",path_usb); //查询卡槽1是否激活
		int result = cmd_recieve_int(cmd);
		if(result != NULL && result == 2) //[卡槽1] 未激活
		{
			value_nextcard = 0;
		}
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s 'AT+SMSIMCFG?' | grep \"+SMSIMCFG: 1,2\" | awk -F ',' '{print $3}'",path_usb); //查询卡槽2是否激活
		result = cmd_recieve_int(cmd);
		if(result != NULL && result == 2)	//[卡槽2] 未激活
		{
			value_nextcard = 1;
		}
		//激活卡槽
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s 'AT+SMSIMCFG=1,%d'",path_usb,(value_nextcard+1));
		cmd_recieve_str(cmd,NULL);
		memset(tmp_buff,0,sizeof(tmp_buff));
		sprintf(tmp_buff,"%d",(value_nextcard+1));
		Debug(tmp_buff,"Set AT+SMSIMCFG");
		sleep(2);
	}
}