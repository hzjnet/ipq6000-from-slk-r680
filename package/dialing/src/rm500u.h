#include "dialing.h"
/* 功能：设定rm500u模组拨号方式
** 返回值：void
*/
void setModule_RM500U(char *tmp_mode_usb,char *tmp_mode_nat)
{
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
	//设定拨号上网驱动
	/* if( tmp_mode_usb != NULL )
	{
		
	} */
	//设定拨号上网方式
	if( tmp_mode_nat != NULL )
	{
		if(strlen(tmp_mode_nat)>0)
		{
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"sendat %s 'AT+QCFG=\"nat\"' | grep '\"nat\",%s' | wc -l",path_usb,tmp_mode_nat);
			tmp_int_buff = cmd_recieve_int(cmd);
			//当前上网方式与设定方式不一致时进行设定
			if (tmp_int_buff == 0)
			{
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"sendat %s 'AT+QCFG=\"nat\",%s' | grep \"OK\" | wc -l",path_usb,tmp_mode_nat);
				tmp_int_buff = cmd_recieve_int(cmd);
				if(tmp_int_buff == 1)
					Debug(tmp_mode_nat,"Set AT+QCFG=nat");
				else
					Debug(tmp_mode_nat,"Failed to set AT+QCFG=nat");
					
			}else{	//一致时，打印当前上网方式
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"sendat %s 'AT+QCFG=\"nat\"' | grep '+QCFG: \"nat\"' | awk -F ',' '{print$2}'",path_usb);
				memset(tmp_str_buff,0,sizeof(tmp_str_buff));
				cmd_recieve_str(cmd,tmp_str_buff);
				Debug(tmp_str_buff,"AT+QCFG=nat");
			}
		}
	}
	//设置支持SIM卡热插拔
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s 'AT+QSIMDET?' | grep '+QSIMDET: 1,1' | wc -l",path_usb);
	tmp_int_buff = cmd_recieve_int(cmd);
	//当前不支持热插拔时
	if (tmp_int_buff == 0)
	{
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s 'AT+QSIMDET=1,1' | grep \"OK\" | wc -l",path_usb,tmp_mode_nat);
		tmp_int_buff = cmd_recieve_int(cmd);
		if(tmp_int_buff == 1)
			Debug("Set AT+QSIMDET=1,1 OK",NULL);
		else
			Debug("Failed -1 to set AT+QSIMDET=1,1",NULL);
			
	}else{	//支持热插拔时
		Debug("AT+QSIMDET=1,1 OK",NULL);
	}
}

/* 功能：解析fm650所处网络小区信息
** 返回值：void
*/
void analyticQENG_RM500U()
{
	char tmp_buff[2048];
	char cmd[1024];
	char nservice[100];
	char band[100];
	int tmp_signal;
	memset(cmd,0,sizeof(cmd));
	memset(tmp_buff,0,sizeof(tmp_buff));
	memset(nservice,0,sizeof(nservice));
	memset(band,0,sizeof(band));
	sprintf(cmd,"sendat %s 'AT+QENG=\"servingcell\"' 500 > %s",path_usb,path_at);
	cmd_recieve_str(cmd,NULL);
	memset(tmp_buff,0,sizeof(tmp_buff));
	sprintf(cmd,"cat %s | grep -A 2 \"servingcell\" | xargs echo -n",path_at);
	cmd_recieve_str(cmd,tmp_buff);
	if(strstr(tmp_buff,"SEARCH"))
	{
		strcpy(nservice,"Searching Service");
		return;
	}
	else if(strstr(tmp_buff,"LIMSRV"))
	{
		strcpy(nservice,"No Service");
		return;
	}
	else{
		//解析服务类型
		if(strstr(tmp_buff,"NR5G-NSA"))
		{
			strcpy(nservice,"NR5G_NSA");
			
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"echo \"%s\" | awk -F ',' '{print $27}'",tmp_buff);
			//memset(band,0,sizeof(band));
			cmd_recieve_str(cmd,band);
			
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"echo \"%s\" | awk -F ',' '{print $30}'",tmp_buff);
			memset(cmd,0,sizeof(cmd));
			tmp_signal=cmd_recieve_int(cmd);
			analyticRSRP(tmp_signal);
		}
		else if(strstr(tmp_buff,"NR5G-SA"))
		{
			strcpy(nservice,"NR5G_SA");
			
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"echo \"%s\" | awk -F ',' '{print $11}'",tmp_buff);
			//memset(band,0,sizeof(band));
			cmd_recieve_str(cmd,band);
			
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"echo \"%s\" | awk -F ',' '{print $13}'",tmp_buff);
			tmp_signal=cmd_recieve_int(cmd);
			analyticRSRP(tmp_signal);
		}
		else if(strstr(tmp_buff,"LTE"))
		{
			strcpy(nservice,"LTE");
			
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"echo \"%s\" | awk -F ',' '{print $10}'",tmp_buff);
			//memset(band,0,sizeof(band));
			cmd_recieve_str(cmd,band);
			
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"echo \"%s\" | awk -F ',' '{print $14}'",tmp_buff);
			tmp_signal=cmd_recieve_int(cmd);
			analyticRSRP(tmp_signal);
		}
		else if(strstr(tmp_buff,"WCDMA"))
		{
			strcpy(nservice,"WCDMA");
			strcpy(band,"WCDMA");
			analyticCSQ();
		}
		if(strlen(nservice)>0)
		{
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"echo \"+CPSI: %s\" >> %s",nservice,path_outfile);
			cmd_recieve_str(cmd,NULL);
			
			if(strlen(band)>0)
			{
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"echo \"+BAND: %s\" >> %s",band,path_outfile);
				cmd_recieve_str(cmd,NULL);
			}
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"%s BAND=%s",nservice,band);
			Debug(cmd,NULL);
		}
	}
}
/* 功能：设定fm650模组上网服务类型
** 返回值：void
*/
void setRATMODE_R500U(char *mode)
{

	char cmd[1024];
	char tmp_buff[1024];
	//设定服务类型
_SETRAT_R500U_AGAIN:	
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s 'AT+QNWPREFCFG=\"mode_pref\",%s'",path_usb,mode);
	memset(tmp_buff,0,sizeof(tmp_buff));
	cmd_recieve_str(cmd,tmp_buff);
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"%s",mode);
	if(strstr(tmp_buff,"OK"))				//获取设定结果，OK
		Debug(cmd,"Set AT+QNWPREFCFG=\"mode_pref\"");
	else if(strstr(tmp_buff,"ERROR"))		//获取设定结果，ERROR
		Debug(cmd,"Failed-1 to set AT+QNWPREFCFG=\"mode_pref\"");
	else{									//获取设定结果失败，通过goto再次设定
		Debug(cmd,"Failed-2 to set AT+QNWPREFCFG=\"mode_pref\"");
		goto _SETRAT_R500U_AGAIN;
	}
	//获取当前拨号状态，处于拨号时挂断以重新拨号
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s \"AT+QNETDEVSTATUS=1\" 1000 | grep \"+QNETDEVSTATUS:\" | wc -l",path_usb);
	int result=cmd_recieve_int(cmd);
	if(result == 1)
	{
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s 'AT+QNETDEVCTL=1,2,1' 1000",path_usb);
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
/* 功能：对fm650模组进行拨号
** 返回值：void
*/
void Dial_R500U()
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
	char tmp_buff3[1024];
	char tmp_buff4[1024];
	memset(tmp_buff1,0,sizeof(tmp_buff1));
	memset(tmp_buff2,0,sizeof(tmp_buff2));
	memset(tmp_buff3,0,sizeof(tmp_buff3));
	memset(tmp_buff4,0,sizeof(tmp_buff4));
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s 'AT+CEREG?'| grep \"0,1\" | wc -l",path_usb);
	int result1=cmd_recieve_int(cmd);
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s 'AT+CEREG?'| grep \"0,5\" | wc -l",path_usb);
	int result2=cmd_recieve_int(cmd);
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s 'AT+C5GREG?'| grep \"0,1\" | wc -l",path_usb);
	int result3=cmd_recieve_int(cmd);
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s 'AT+C5GREG?'| grep \"0,5\" | wc -l",path_usb);
	int result4=cmd_recieve_int(cmd);
	//CEREG：0,1 0,5 时尝试拨号
	if( result1 ==1 || result2 ==1 || result3 ==1 || result4 ==1)
	{
		//判断是否处于已拨号状态
_DIAL_R500U_AGAIN:
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s \"AT+QNETDEVSTATUS=1\" 1000 | grep \"+QNETDEVSTATUS:\" | wc -l",path_usb);
		int result3=cmd_recieve_int(cmd);
		if(result3 ==1 )	//是，重启网卡
		{
			cmd_recieve_str(cmd_ifup,NULL);
			Debug(cmd_ifup,NULL);
			Debug("Dial R500U OK",NULL);
		}
		else{				//否，尝试拨号
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"sendat %s \"AT+QNETDEVCTL=1,3,1\" > /dev/null 2>&1",path_usb);
			cmd_recieve_str(cmd,NULL);
			sleep(1);
			if(count++<3){	//通过goto再次判断,最多3次
				goto _DIAL_R500U_AGAIN;
			}else{			//3次后判定拨号失败
				Debug("Dial R500U Fialed",NULL);
				count=0;
			}
		}
	}
	return;
}