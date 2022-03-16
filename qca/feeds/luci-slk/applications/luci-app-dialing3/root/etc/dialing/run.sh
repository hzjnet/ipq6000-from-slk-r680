#!/bin/sh 

. /etc/dialing/fun_modem

i=0
xd=0


#/etc/config/modem
# config ndis
	# option enable '1'
	# option bandlist '0'
	# option smode '0'
	# option pingaddr '114.114.114.114'
	# option pingen '1'
	# option an '10'
	# option model '0'
	# option apn '3gnet'		##3gnet/cmnet/cnnet
	# option auth_type 'none'	##PAP/CHAP/NONE/PAP-CHAP
	# option pincode '1234'
	# option username 'card'
	# option password 'card'
	# option ipv4v6	'ipv4'		##IPV4 or IPV4/V6 
	

##############读取系统配置##############
READ_Config()
{
	version=$(cat /proc/version | grep "OpenWrt GCC 7.3.0" | wc -l)
	if [ $version -ge 1 ]; then
			#延迟读取配置
			sleep 2s
	fi
	dial_en=$(uci -q get modem.@ndis[0].enable)				# 拔号使能
	force_dial=$(uci -q get modem.@ndis[0].force_dial)      # 强制拨号
	apn=$(uci -q get modem.@ndis[0].apn)					# APN	
	smode=$(uci -q get modem.@ndis[0].smode)				# 工作模式
	pin_code=$(uci -q get modem.@ndis[0].pincode)			# PIN码
	auth=$(uci -q get modem.@ndis[0].auth_type)				# AUTH认证
	username=$(uci -q get modem.@ndis[0].username)			# 用户名
	password=$(uci -q get modem.@ndis[0].password)			# 密码
	iptype=$(uci -q get modem.@ndis[0].ipv4v6)				# IPV4/V6
	ping_en=$(uci -q get modem.@ndis[0].pingen)				# PING命令使能
	ping_addr=$(uci -q get modem.@ndis[0].pingaddr)			# PING地址
	ping_count=$(uci -q get modem.@ndis[0].count)			# PING次数
	usbmode=$(uci -q get modem.@ndis[0].usbmode)			# 网卡驱动方式
	natmode=$(uci -q get modem.@ndis[0].natmode)			# 网卡拨号方式
	Debug "READ_Config"
}

##############配置模块驱动##############
Set_Model()
{
	if [ "$model" == "fm650" ];then
		case $usbmode in
			"1") um=35	# 35 ECM
			;;
			"3") um=39	# 39 RNDIS
			;;
			"5") um=36	# 36 NCM
			;;
			*) um=36	# 36 NCM
			;;
		esac
		case $natmode in
			"0") nm=1	# 1 网卡模式
			;;
			"1") nm=0	# 0 路由模式
			;;
			*) nm=1		# 1 网卡模式
			;;
		esac
		Set_FM650_MODEL "$um" "$nm" 		#配置FM650网卡
	elif [ "$model" == "rm500u" ];then
		Set_RM500U_MODEL $usbmode $natmode 	#配置RM500U网卡
	fi
}

##############读模块状态##############
Modem_Info()
{
	check_imei		#读取IMEI
	check_cpin 		#读取SIM卡
	check_imsi		#读取IMSI 
	check_cops		#读取运行商
	check_csq		#读取信号值
	if [ "$model" == "mv31w" ]; then
		Analytic_MV31W_DEBUG	#解析Debug命令
	elif [ "$model" == "sim7600" ]; then
		Analytic_SIMCOM_CPSI	#解析CPSI命令
	elif [ "$model" == "sim8200" ]; then
		Analytic_SIMCOM_CPSI	#解析CPSI命令
	elif [ "$model" == "fm650" ]; then
		Analytic_FM650_INFO		#解析INFO命令
	elif [ "$model" == "rm500u" ]; then
		Analytic_RM500U_QENG	#解析QENG命令
	fi
	cat /tmp/modem > /tmp/modem2
}
###########设置模组拨号信息##############
Set_Modem()
{	
	if [ "$i" -ne "0" ]; then
		return
	fi
	Debug "Set modem config"
	
	#设置PIN码
	if [ -n "$pin_code" ];then 
		Set_pincode "$pin_code"	
	fi
	
	if [ "$model" == "rm500u" ];then																#rm500u配置APN,用户名和密码
		#选择IP协议
		if [ "$iptype" == "IPV4" ];then
			ipt=1
		elif [ "$iptype" == "IPV6" ];then
			ipt=2
		elif [ "$iptype" == "IPV4V6" ];then
			ipt=3
		fi
		#设置APN
		sendat $usb_path "AT+QICSGP=1,$ipt,\"$apn\",\"$username\",\"$password\",$auth" > $file
		rec=$(cat $file |grep "OK" | wc -l)
		if [ "$rec" -eq "1" ]; then
			Debug "Set $iptype:$ipt,APN:$apn,USERNAME:$username,PWD:$password,AUTH:$auth"
		fi
	else																							#sim8200\sim7600\mv31w\fm650配置APN
		#配置默认值															
		sendat $usb_path "AT+CGDCONT=1,\"IP\"" > /dev/null 2>&1										
		
		#设置APN
		if [ -n "$apn" ];then 
			sendat $usb_path "AT+CGDCONT=1,\"$iptype\",\"$apn\"" > /dev/null 2>&1
			
			if [ "$model" == "mv31w" ]; then 														#mv31w配置用户名和密码
				sendat $usb_path "AT$QCPDPP=1,$auth,\"$password\",\"$username\"" 500 > $file			#测试无法设置
				rec=$(cat $file |grep "OK")
				if [ -n "$rec" ]; then
					Debug "Set AT$QCPDPP=1,$auth,$password,$username"
				fi
			elif [ "$model" == "fm650" ] && [ -n $username ]; then 									#fm650配置用户名和密码
				sendat $usb_path "AT+MGAUTH=1,$auth,\"$username\",\"$password\"" 500 > $file
				rec=$(cat $file |grep "OK")
				if [ -n "$rec" ]; then
					Debug "Set AT+MGAUTH=1,$auth,$username,$password"
				fi
			fi
		else
			sendat $usb_path "AT+CGDCONT=1,\"$iptype\"" > /dev/null 2>&1
		fi
	fi
	
	#设置工作模式
	if [ "$model" == "mv31w" ]; then
		if [ -n "$smode" ];then
			MV31W_SLMODE "$smode" 
		fi
	elif [ "$model" == "sim7600" ]; then
		case $smode in
			"0") xx=2					# 2 Automatically
			;;
			"1") xx=14					# 14 WCDMA Only
			;;
			"2") xx=38					# 38 LTE Only
			;;
			"3") xx=54					# 54 WCDMA And LTE 
			;;
			"4") xx=71					# 71 NR5G Only
			;;
			"5") xx=13					# 13 GSM Only
			;;
			"6") xx=109					# 109 LTE And NR5G
			;;
			"7") xx=55					# 55 WCDMA And LTE And NR5G
			;;
			"8") xx=19					# 19 GSM And WCDMA
			;;
			"9") xx=51					# 51 GSM And LTE
			;;
			"10") xx=39					# 39 GSM And WCDMA AND LTE
			;;
			*) xx=2						# 2 Automatically
			;;
		esac
		SIMCOM_MODE "$xx" 
	elif [ "$model" == "sim8200" ]; then
		case $smode in
			"0") xx=2					# 2 Automatically
			;;
			"1") xx=14					# 14 WCDMA Only
			;;
			"2") xx=38					# 38 LTE Only
			;;
			"3") xx=54					# 54 WCDMA And LTE 
			;;
			"4") xx=71					# 71 NR5G Only
			;;
			"6") xx=109					# 109 LTE And NR5G
			;;
			"7") xx=55					# 55 WCDMA And LTE And NR5G
			;;
			*) xx=2						# 2 Automatically
			;;
		esac
		SIMCOM_MODE "$xx"
	elif [ "$model" == "fm650" ]; then
		case $smode in
			"0") xx=10					# 10 Automatically
			;;
			"1") xx=2					# 2 WCDMA Only
			;;
			"2") xx=3					# 3 LTE Only
			;;
			"3") xx=4					# 4 WCDMA And LTE 
			;;
			"4") xx=14					# 14 NR5G Only
			;;
			"6") xx=16					# 16 LTE And NR5G
			;;
			"7") xx=20					# 20 WCDMA And LTE And NR5G
			;;
			*) xx=10					# 10 Automatically
			;;
		esac
		FIBOCOM_MODE "$xx"
	elif [ "$model" == "rm500u" ]; then
		case $smode in
			"0") xx="AUTO"				# Automatically
			;;
			"1") xx="WCDMA"				# WCDMA Only
			;;
			"2") xx="LTE"				# LTE Only
			;;
			"3") xx="LTE:WCDMA"			# PREFER LTE,THEN WCDMA
			;;
			"4") xx="NR5G"				# NR5G Only
			;;
			"6") xx="NR5G:LTE"			# PREFER NR5G,THEN LTE
			;;
			"7") xx="NR5G:LTE:WCDMA"	# PREFER NR5G,THEN LTE,LAST WCDMA
			;;
			*) xx="AUTO"				# Automatically
			;;
		esac
		QUECTEL_MODE "$xx"
	fi
	
	if [ "$force_dial" -eq 1 ];then
		Debug "Force Dial is Opened"
	fi
}

##############拔号上网##############
Dial()
{
	if [ "$dial_en" == "1" ]; then																								#拔号开关
		rec=$sval
		if [ "$rec" -ge "2" ] || [ "$force_dial" -eq 1 ]; then																	#信号大于2格或者强制方能拔号
		#Debug "Signal Sval > 2 ; force_dial is $force_dial"
			if [ "$model" == "mv31w" ]; then
				rec=$(ifconfig wwan0 |grep "Mask:")																				#判断wwan0无IP才拔号
				if [ -z "$rec" ]; then
					#Debug "wwan0 not ip"
					MV31W_MBIM
				fi
			elif [ "$model" == "sim8200" ]; then
				rec=$(ifconfig qmimux0 |grep "Mask:")																			#判断qmimux0无IP才拔号
				if [ -z "$rec" ]; then
					Debug "sim8200 dial"
					simcom_cm1
				fi
			elif [ "$model" == "sim7600" ]; then
				rec=$(ifconfig wwan0 |grep "Mask:")																				#判断wwan0无IP才拔号
				if [ -z "$rec" ]; then
					Debug "sim7600 dial"
					RndisAT
				fi
			elif [ "$model" == "fm650" ]; then
				diaip=$(sendat $usb_path AT+GTRNDIS? | grep "+GTRNDIS: 1" | awk -F ',' '{print$3}'| awk -F '"' '{print$2}') 	#查看拨号IP
				ifip=$(ifconfig usb0 | grep "inet addr" | awk -F ':' '{print$2}' | awk -F ' ' '{print$1}')						#查看接口IP
				#注网后拨号
				if [ -n "$diaip" ];then
					rec1=$(sendat $usb_path 'AT+CEREG?'| grep "0,1" | wc -l)
					rec2=$(sendat $usb_path 'AT+CEREG?'| grep "0,5" | wc -l)
					if [ "$rec1" != "1" ] && [ "$rec2" != "1" ]; then
						sendat $usb_path "AT+GTRNDIS=0,1" > /dev/null 2>&1
						Modem_reset
						return
					fi
				fi
				if [ -z "$diaip" ]; then 																						#拨号IP不存在重新拨号
					Debug "fm650 dial"
					GTRndis
				fi
				if [ "$diaip" != "$ifip" ];then 																				#两个IP不同时重启接口
					Debug "ifup modem"
					ifup modem
				fi
			elif [ "$model" == "rm500u" ]; then
				rec=$(ifconfig usb0 |grep "Mask:")																				#判断usb0无IP才拔号
				if [ -z "$rec" ]; then
					Debug "rm500u dial"
					QNRndis
				fi
			fi
			
		fi
	fi
}

#PPP拨号
PPPDial()
{
	if [ "$i" -eq "0" ];then
		uci set network.modem.proto='3g'
		uci set network.modem.device='/dev/ttyUSB2'
		uci set network.modem.service='umt_only'
		if [ -n $apn ];then
			uci set network.modem.apn=$apn
		fi
		if [ -n $pincode ];then
			uci set network.modem.pincode=$pin_code
		fi
		if [ -n $username ];then
			uci set network.modem.username=$username
		fi
		if [ -n $password ];then
			uci set network.modem.password=$password
		fi
		#uci -q delete network.modem.ifname
		#uci commit
		ifup modem
	fi
}

#6、检查拔号状态
Loop_status()
{
	wwan0_ip=$(ifconfig wwan0 |grep "inet addr:")
	if [ -n "$wwan0_ip" ]; then
		Debug "$wwan0_ip"
	else
		Debug "No ip!"
	fi
}

#8、检测网络状态
chenk_dns()
{
	if [ "$dial_en" -eq 1 ] && [ "$ping_en" -eq 1 ] ;then
		if [ "$model" == "mv31w" ]; then
			ifname="wwan0"
		elif [ "$model" == "sim7600" ]; then
			ifname="wwan0"
		elif [ "$model" == "sim8200" ]; then
			ifname="qmimux0"
		elif [ "$model" == "fm650" ]; then
			ifname="usb0"
		elif [ "$model" == "rm500u" ]; then
			ifname="usb0"
		fi
		ping -c 1 -w 1 -I $ifname $ping_addr > /dev/null 2>&1
		if [ $? -eq 0 ];then  
			Debug "网络连接正常" 
			xd=0
		else  
			Debug "网络连接异常:$xd" 
			let xd++
		fi	
	fi
	if [ "$xd" == "$ping_count" ];then
		xd=0
		Debug "Abnormal network restart"
		Modem_reset	#重启模块
	fi
}


##############主函数##############
#1、读取系统配置
#2、读取模组型号
#3、读取模组状态
#4、读取模组状态
#5、控制LED信号指示灯
#6、拔号上网
#7、检查拔号状态
##################################

#关闭Modem接口
ifdown modem
kill_exe simcom-cm

#1、读取系统配置
READ_Config	

#2、读取模组型号
Modem_model	

#2.5、设置模组拨号驱动
Set_Model

while [ 1 ]
do
	echo "####$model####" > $output_file	#清空文件内容
	
	#3、读取模组状态
	Modem_Info		
	
	#4、设置模组拨号信息
	Set_Modem
	
	#5、控制LED信号指示灯
	Signal_LED
	
	#6、拔号上网
	Dial
	
	#7、检查拔号状态
	#Loop_status
	
	#8、检查拔号状态
	chenk_dns
	
	#延迟循环执行
	sleep 5s
	Debug "While loop $i"
	let i++
done
