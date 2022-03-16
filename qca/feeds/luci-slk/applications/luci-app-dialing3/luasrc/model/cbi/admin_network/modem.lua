-- Copyright 2012 Jo-Philipp Wich <jow@openwrt.org>
-- Licensed to the public under the Apache License 2.0.

local m, section, m2, s2

m = Map("modem", translate("SIM Network"))
section = m:section(NamedSection, "SIM", "rndis", translate("SIM Settings"))
section.anonymous = true
section.addremove = false
	section:tab("general", translate("General Settings"))
	section:tab("advanced", translate("Advanced Settings"))
	section:tab("physical", translate("Physical Settings"))

local function set_status(self)
	-- if current network is empty, print a warning
	--if not net:is_floating() and net:is_empty() then
	--	st.template = "cbi/dvalue"
	--	st.network  = nil
	--	st.value    = translate("There is no device assigned yet, please attach a network device in the \"Physical Settings\" tab")
	--else
		st.template = "admin_network/iface_status"
		st.network  = self
		st.value    = nil
	--end
end

st = section:taboption("general", DummyValue, "__statuswifi", translate("Status"))
st.on_init = set_status("SIM")

enable = section:taboption("general", Flag, "enable", translate("Enable"))
enable.rmempty  = false

--查询模块型号--
function modem_name(name)
	name=string.gsub(luci.sys.exec("cat /tmp/SIM | grep "..name.." | wc -l"),"%s+","")
	return name
end


apn = section:taboption("general", Value, "apn", translate("APN"))

username = section:taboption("general", Value, "username", translate("Username"))

password = section:taboption("general", Value, "password", translate("Password"))
password.password = true

auth_type = section:taboption("general", ListValue, "auth_type", translate("Auth Type"))
auth_type:value("0", translate"none")
auth_type:value("1", "pap")
auth_type:value("2", "chap")
if modem_name("mv31w") == "1" then
	auth_type:value("3", "mschapv2")
elseif modem_name("fm650") == "1" then
	auth_type:value("3", "pap/chap")
elseif modem_name("rm500u") == "1" then
	auth_type:value("3", "pap/chap")
elseif modem_name("sim7600") == "1" then
	auth_type:value("3", "pap/chap")
end

pincode = section:taboption("general", Value, "pincode", translate("PIN Code"))
---------------------------advanced------------------------------
enable = section:taboption("advanced", Flag, "force_dial", translate("Force Dial"))
enable.rmempty  = false

ipv4v6 = section:taboption("advanced", ListValue, "ipv4v6", translate("IP Type"))
ipv4v6:value("IP", "IPV4")
ipv4v6:value("IPV6", "IPV6")
ipv4v6:value("IPV4V6", "IPV4V6")

bandlist = section:taboption("advanced", ListValue, "bandlist", translate("Lock Band List"))

bandlist:value("0", translate("Disabled"))

smode = section:taboption("advanced", ListValue, "smode", translate("Server Type"))
smode.default = "0"
smode:value("0", translate"Automatically")
smode:value("1", "WCDMA Only")
smode:value("2", "LTE Only")
smode:value("3", "WCDMA And LTE")
if modem_name("sim7600") == "1" then
	smode:value("5", "GSM Only")
	smode:value("8", "GSM And WCDMA")
	smode:value("9", "GSM And LTE")
	smode:value("10", "GSM And WCDMA And LTE")
else
	smode:value("4", "NR5G Only")
	smode:value("6", "LTE And NR5G")
	smode:value("7", "WCDMA And LTE And NR5G")
end

default_card = section:taboption("physical", ListValue, "default_card", translate("Default SIM Card"))
default_card:value("0", translate"SIM1")
default_card:value("1", translate"SIM2")
default_card.default = "0"

metric = section:taboption("physical", Value, "metric", translate("Metric"))
metric.default = "10"

mtu = section:taboption("physical", Value, "mtu", translate("Override MTU"))
mtu.placeholder = "1400"
mtu.datatype = "max(9200)"

s2 = m:section(NamedSection, "SIM", "rndis", translate("Abnormal Restart"),translate("Network exception handling: \
check the network connection in a loop for 5 seconds. If the Ping IP address is not successful, After the network \
exceeds the abnormal number, restart and search the registered network again."))
s2.anonymous = true
s2.addremove = false

en = s2:option(Flag, "pingen", translate("Enable"))
en.rmempty = false

ipaddress= s2:option(Value, "pingaddr", translate("Ping IP address"))
ipaddress.default = "114.114.114.114"
ipaddress.rmempty=false

opera_mode = s2:option(ListValue, "opera_mode", translate("Operating mode"))
opera_mode.default = "airplane_mode"
opera_mode:value("reboot_route", translate("Reboot on internet connection lost"))
opera_mode:value("airplane_mode", translate("Set airplane mode"))
opera_mode:value("switch_card", translate("Switch SIM card"))

an = s2:option(Value, "count", translate("Abnormal number"))
an.default = "15"
an:value("3", "3")
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
