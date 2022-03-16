-- Copyright 2012 Jo-Philipp Wich <jow@openwrt.org>
-- Licensed to the public under the Apache License 2.0.

local m, section, m2, s2

m = Map("modem", translate("Mobile Network"))
section = m:section(TypedSection, "ndis", translate("SIM Settings"))
section.anonymous = true
section.addremove = false
	section:tab("general", translate("General Setup"))
	section:tab("advanced", translate("Advanced Settings"))


enable = section:taboption("general", Flag, "enable", translate("Enable"))
enable.rmempty  = false

apn = section:taboption("general", Value, "apn", translate("APN"))

username = section:taboption("general", Value, "username", translate("Username"))

password = section:taboption("general", Value, "password", translate("Password"))
password.password = true

pincode = section:taboption("general", Value, "pincode", translate("PIN Code"))

---------------------------advanced------------------------------

bandlist = section:taboption("advanced", ListValue, "bandlist", translate("Lock Band List"))
if (string.gsub(luci.sys.exec('uci get system.@system[0].modem |grep lte |wc -l'),"%s+","")=="1") then
bandlist.default = "0"
bandlist:value("1", "LTE BAND1")
bandlist:value("2", "LTE BAND2")
bandlist:value("3", "LTE BAND3")
bandlist:value("4", "LTE BAND4")
bandlist:value("5", "LTE BAND5")
bandlist:value("7", "LTE BAND7")
bandlist:value("8", "LTE BAND8")
bandlist:value("20", "LTE BAND20")
bandlist:value("38", "LTE BAND38")
bandlist:value("40", "LTE BAND40")
bandlist:value("41", "LTE BAND41")
bandlist:value("28", "LTE BAND28")
bandlist:value("A", "AUTO")
end
bandlist:value("0", translate("Disable"))

servertype = section:taboption("advanced", ListValue, "servertype", translate("Server Type"))
servertype.default = "0"

if (string.gsub(luci.sys.exec('uci get system.@system[0].modem |grep nr5g |wc -l'),"%s+","")=="1") then
	servertype:value("1", "5G Only")
	servertype:value("5", "4G/5G Only")
end
servertype:value("2", "4G Only")
servertype:value("3", "3G Only")
servertype:value("4", "2G Only")
servertype:value("0", "AUTO")

s2 = m:section(TypedSection, "ndis", translate("Network Diagnostics"),translate("Network exception handling: \
check the network connection in a loop for 5 seconds. If the Ping IP address is not successful, After the network \
exceeds the abnormal number, restart and search the registered network again."))
s2.anonymous = true
s2.addremove = false

en = s2:option(Flag, "en", translate("Enable"))
en.rmempty = false

ipaddress= s2:option(Value, "ipaddress", translate("Ping IP address"))
ipaddress.default = "114.114.114.114"
ipaddress.rmempty=false

an = s2:option(Value, "an", translate("Abnormal number"))
an.default = "15"
an:value("1", "1")
an:value("5", "5")
an:value("10", "10")
an:value("15", "15")
an:value("20", "20")
an:value("25", "25")
an:value("30", "30")
an.rmempty=false

local apply = luci.http.formvalue("cbi.apply")
if apply then
    io.popen("/etc/init.d/modeminit restart")
end

return m,m2
