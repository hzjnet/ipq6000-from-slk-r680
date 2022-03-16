#include "dialing.h"
/* 功能：设定fm650模组拨号方式
** 返回值：void
*/
void setModule_FM650(char *tmp_mode_usb,char *tmp_mode_nat)
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
			sprintf(cmd,"sendat %s \"AT+GTIPPASS?\" | grep \"+GTIPPASS: %s\" | wc -l",path_usb,tmp_mode_nat);
			tmp_int_buff = cmd_recieve_int(cmd);
			//当前上网方式与设定方式不一致时进行设定
			if (tmp_int_buff == 0)
			{
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"sendat %s \"AT+GTIPPASS=%s\" 500 | grep \"OK\" |wc -l",path_usb,tmp_mode_nat);
				tmp_int_buff = cmd_recieve_int(cmd);
				if(tmp_int_buff == 1)
					Debug(tmp_mode_nat,"Set AT+GTIPPASS");
				else
					Debug(tmp_mode_nat,"Failed to set AT+GTIPPASS");
					
			}else{	//一致时，打印当前上网方式
				memset(cmd,0,sizeof(cmd));
				sprintf(cmd,"sendat %s \"AT+GTIPPASS?\" | grep \"+GTIPPASS: \" | awk -F ' ' '{print$2}'",path_usb);
				memset(tmp_str_buff,0,sizeof(tmp_str_buff));
				cmd_recieve_str(cmd,tmp_str_buff);
				Debug(tmp_str_buff,"AT+GTIPPASS");
			}
		}
	}
	//设定当前读卡位置
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s 'AT+GTDUALSIM?' | grep \"+GTDUALSIM:\" | awk -F ',' '{print $1}' | awk -F ' ' '{print $2}'",path_usb);
	int result = cmd_recieve_int(cmd);
	//printf("Current card slot:%d\n",result);
	if(value_defaultcard == result) //[优先卡槽] == [当前卡槽]
	{
		memset(tmp_str_buff,0,sizeof(tmp_str_buff));
		sprintf(tmp_str_buff,"%d",result);
		Debug(tmp_str_buff,"AT+GTDUALSIM");
	}else
	{
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s 'AT+GTDUALSIM=%d'",path_usb,value_defaultcard);
		cmd_recieve_str(cmd,NULL);
		memset(tmp_str_buff,0,sizeof(tmp_str_buff));
		sprintf(tmp_str_buff,"%d",value_defaultcard);
		Debug(tmp_str_buff,"Set AT+GTDUALSIM");
		sleep(5);
	}
}
/* 功能：解析CESQ，转换为信号值
** 返回值：void
*/
void analyticCESQ()
{
	char tmp_buff[1024];
	char cmd[1024];
	memset(cmd,0,sizeof(cmd));
	memset(tmp_buff,0,sizeof(tmp_buff));
	sprintf(cmd,"sendat %s \"AT+CESQ\" | grep \"+CESQ:\"",path_usb);
	cmd_recieve_str(cmd,tmp_buff);
	if(strstr(tmp_buff,"+CESQ:"))
	{
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"echo \"%s\" | awk '{print $2}' | awk -F ',' '{print $8}'",tmp_buff);
		int tmp_csq;
		tmp_csq=cmd_recieve_int(cmd);
		if(tmp_csq == 255 )				//255		无信号
			value_signal=0;
		else if(tmp_csq < 16 )			//0-15		无信号
			value_signal=0;
		else if(tmp_csq < 35 )			//16-34		1格
			value_signal=1;
		else if(tmp_csq < 45 )			//35-44		2格
			value_signal=2;
		else if(tmp_csq < 55 )			//45-54 	3格
			value_signal=3;
		else if(tmp_csq < 65 )			//55-64		4格
			value_signal=4;
		else if(tmp_csq < 135 )			//65-135 	5格
			value_signal=5;
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"echo \"+SVAL: %d\" >> %s",value_signal,path_outfile);
		cmd_recieve_str(cmd,NULL);
		
		memset(tmp_buff,0,sizeof(tmp_buff));
		sprintf(tmp_buff,"SS_RSSI: %d SVAL: %d",tmp_csq,value_signal);
		Debug(tmp_buff,NULL);
	}
}

/* 功能：解析fm650所处网络小区信息
** 返回值：void
*/
void analyticINFO_FM650()
{
	char tmp_buff[2048];
	char cmd[1024];
	char nservice[100];
	char band[100];
	memset(cmd,0,sizeof(cmd));
	memset(tmp_buff,0,sizeof(tmp_buff));
	memset(nservice,0,sizeof(nservice));
	memset(band,0,sizeof(band));
	sprintf(cmd,"sendat %s \"AT+GTCCINFO?\" 500 > %s",path_usb,path_at);
	cmd_recieve_str(cmd,NULL);
	memset(tmp_buff,0,sizeof(tmp_buff));
	sprintf(cmd,"cat %s | grep -A 2 \"service cell\" | xargs echo -n",path_at);
	cmd_recieve_str(cmd,tmp_buff);
	if(strstr(tmp_buff,"service cell"))
	{
		//解析服务类型
		if(strstr(tmp_buff,"LTE-NR"))
		{
			strcat(nservice,"NR5G_NSA");
		}else{
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"echo \"%s\" | awk -F ',' '{print $2}'",tmp_buff);
			char tmp_nservice[5];
			memset(tmp_nservice,0,sizeof(tmp_nservice));
			cmd_recieve_str(cmd,tmp_nservice);
			if(strcmp(tmp_nservice,"0")==0)
			{
				Debug("NO SERVICE",NULL);
				return;
			}else if(strcmp(tmp_nservice,"1")==0)
				strcat(nservice,"GSM");
			else if(strcmp(tmp_nservice,"2")==0)
				strcat(nservice,"WCDMA");
			else if(strcmp(tmp_nservice,"3")==0)
				strcat(nservice,"TDSCDMA");
			else if(strcmp(tmp_nservice,"4")==0)
				strcat(nservice,"LTE");
			else if(strcmp(tmp_nservice,"5")==0)
				strcat(nservice,"eMTC");
			else if(strcmp(tmp_nservice,"6")==0)
				strcat(nservice,"NB-IoT");
			else if(strcmp(tmp_nservice,"7")==0)
				strcat(nservice,"CDMA");
			else if(strcmp(tmp_nservice,"8")==0)
				strcat(nservice,"EVDO");
			else if(strcmp(tmp_nservice,"9")==0)
				strcat(nservice,"NR5G");
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"echo \"+CPSI: %s\" >> %s",nservice,path_outfile);
			cmd_recieve_str(cmd,NULL);
				
		}
		//解析上网频段
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"echo \"%s\" | awk -F ',' '{print $9}'",tmp_buff);
		cmd_recieve_str(cmd,band);
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"echo \"+BAND: %s\" >> %s",band,path_outfile);
		cmd_recieve_str(cmd,NULL);
			
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"%s BAND=%s",nservice,band);
		Debug(cmd,NULL);
			
		if( strstr(nservice,"NR5G") || strstr(nservice,"NR5G_NSA") )	//5G解析CESQ
			analyticCESQ();
		else
			analyticCSQ();												//其他解析CSQ
	}
}
/* 功能：设定fm650模组上网服务类型
** 返回值：void
*/
void setRATMODE_FM650(int mode)
{

	char cmd[1024];
	char tmp_buff[1024];
	//设定服务类型
_SETRAT_FM650_AGAIN:	
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s \"AT+GTRAT=%d\"",path_usb,mode);
	memset(tmp_buff,0,sizeof(tmp_buff));
	cmd_recieve_str(cmd,tmp_buff);
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"%d",mode);
	if(strstr(tmp_buff,"OK"))				//获取设定结果，OK
		Debug(cmd,"Set AT+GTRAT");
	else if(strstr(tmp_buff,"ERROR"))		//获取设定结果，ERROR
		Debug(cmd,"Failed-1 to set AT+GTRAT");
	else{									//获取设定结果失败，通过goto再次设定
		Debug(cmd,"Failed-2 to set AT+GTRAT");
		goto _SETRAT_FM650_AGAIN;
	}
	//获取当前拨号状态，处于拨号时挂断以重新拨号
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"sendat %s \"AT+GTRNDIS?\" 1000 | grep \"+GTRNDIS: 1\" | wc -l",path_usb);
	int result=cmd_recieve_int(cmd);
	if(result == 1)
	{
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s 'AT+GTRNDIS=0,1' 1000",path_usb);
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
void Dial_FM650()
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
	//CEREG：0,1 0,5 时尝试拨号
	if( result1 ==1 || result2 ==1)
	{
		//判断是否处于已拨号状态
_DIAL_FM650_AGAIN:
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s \"AT+GTRNDIS?\" 1000 | grep \"+GTRNDIS: 1\" | wc -l",path_usb);
		int result3=cmd_recieve_int(cmd);
		if(result3 ==1 )	//是，重启网卡
		{
			cmd_recieve_str(cmd_ifup,NULL);
			Debug(cmd_ifup,NULL);
			Debug("Dial FM650 OK",NULL);
		}
		else{				//否，尝试拨号
			memset(cmd,0,sizeof(cmd));
			sprintf(cmd,"sendat %s \"AT+GTRNDIS=1,1\" > /dev/null 2>&1",path_usb);
			cmd_recieve_str(cmd,NULL);
			sleep(1);
			if(count++<3){	//通过goto再次判断,最多3次
				goto _DIAL_FM650_AGAIN;
			}else{			//3次后判定拨号失败
				Debug("Dial FM650 Fialed",NULL);
				count=0;
			}
		}
	}
	return;
}

void SwitchSIM_FM650(bool isdefault)
{
	char cmd[1024];
	char tmp_buff[1024];
	int value_nextcard=value_defaultcard;
	if(isdefault) //设置默认卡槽
	{
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s 'AT+GTDUALSIM=%d'",path_usb,value_defaultcard);
		cmd_recieve_str(cmd,NULL);
		sprintf(tmp_buff,"%d",value_defaultcard);
		Debug(tmp_buff,"Set AT+GTDUALSIM");
		sleep(3);
	}
	else 		//获取当前卡槽并切换卡槽
	{
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s 'AT+GTDUALSIM?' | grep \"+GTDUALSIM:\" | awk -F ',' '{print $1}' | awk -F ' ' '{print $2}'",path_usb); //获取当前卡槽
		int value_currentcard = cmd_recieve_int(cmd); 
		if(value_currentcard == 0) 			//[当前卡槽] 0
			value_nextcard = 1;
		else if(value_currentcard == 1)		//[当前卡槽] 1
			value_nextcard = 0;
		memset(tmp_buff,0,sizeof(tmp_buff));
		sprintf(tmp_buff,"%d",value_currentcard);
		Debug(tmp_buff,"Current card slot");
		//切换卡槽
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"sendat %s 'AT+GTDUALSIM=%d'",path_usb,value_nextcard);
		cmd_recieve_str(cmd,NULL);
		memset(tmp_buff,0,sizeof(tmp_buff));
		sprintf(tmp_buff,"%d",value_nextcard);
		Debug(tmp_buff,"Set AT+GTDUALSIM");
		sleep(3);
	}
}