#include "dialing.h"
/* 功能：解析sim7600所处网络小区信息
** 返回值：void
*/
void analyticCPSI_SIM7600()
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
		if(strstr(tmp_buff,"LTE"))
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
/* 功能：设定sim7600模组上网服务类型
** 返回值：void
*/
void setRATMODE_SIM7600(int mode)
{

	char cmd[1024];
	char tmp_buff[1024];
	//设定服务类型
_SETRAT_SIM7600_AGAIN:	
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
		goto _SETRAT_SIM7600_AGAIN;
	}
	//获取当前拨号状态，处于拨号时挂断以重新拨号
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s 'AT$QCRMCALL?' 1000 | grep \"$QCRMCALL:\" | grep \"V4\" | wc -l",path_usb);
	int result=cmd_recieve_int(cmd);
	if(result == 1)
	{
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s 'AT$QCRMCALL=0,1' 1000",path_usb);
		memset(tmp_buff,0,sizeof(tmp_buff));
		cmd_recieve_str(cmd,tmp_buff);
		if(strstr(tmp_buff,"OK"))
			Debug("Close dial",NULL);
		else if(strstr(tmp_buff,"ERROR"))
			Debug("Failed-1 to close dial",NULL);
		else
			Debug("Failed-2 to close dial",NULL);
	}
	
}
/* 功能：对sim7600模组进行拨号
** 返回值：void
*/
void Dial_SIM7600()
{
	int count=0;
	char cmd[1024];
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
	//获取SIM卡当前驻网状态
	char tmp_buff1[1024];
	char tmp_buff2[1024];
	memset(tmp_buff1,0,sizeof(tmp_buff1));
	memset(tmp_buff2,0,sizeof(tmp_buff2));
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s 'AT+CEREG?'| grep \"0,1\" | wc -l",path_usb);
	int result1=cmd_recieve_int(cmd);
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s 'AT+CEREG?'| grep \"0,5\" | wc -l",path_usb);
	int result2=cmd_recieve_int(cmd);
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s 'AT+CEREG?'| grep \"0,4\" | wc -l",path_usb);
	int result3=cmd_recieve_int(cmd);
	//CEREG：0,1 0,5 0,4时尝试拨号
	if( result1 ==1 || result2 ==1 || result3 ==1)
	{
		if(strlen(value_apn)>0)
		{
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"sendat %s 'AT$QCRMCALL=1,1,%s,,,,\"%s\",\"%s\",\"s\",%s' 1000 | grep \"$QCRMCALL:\" | grep \"V4\" | wc -l",path_usb,type_ip,value_apn,value_username,value_password,value_auth);
		}else
		{
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"sendat %s 'AT$QCRMCALL=1,1' 1000 | grep \"$QCRMCALL:\" | grep \"V4\" | wc -l",path_usb);
		}
		int result=cmd_recieve_int(cmd);
		if(result ==1 )	//是，重启网卡
		{
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"Dial SIM7600 OK,APN %s,USERNAME %s,PASSWORD %s,AUTH %s,IP_TYPE %s",value_apn,value_username,value_password,value_auth,type_ip);
			Debug(cmd,NULL);
			cmd_recieve_str(cmd_ifup,NULL);
			Debug(cmd_ifup,NULL);
		}
		else{				//否
			Debug("Dial SIM7600 Fialed",NULL);
		}
	}
	return;
}