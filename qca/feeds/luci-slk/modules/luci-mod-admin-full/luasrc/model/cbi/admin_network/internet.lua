-- Copyright 2012 Christian Gagneraud <chris@techworks.ie>
-- Licensed to the public under the Apache License 2.0.

--local nw = require "luci.model.network"
--local net = nw:get_network("wlan0")

m = Map("network",
	translate("Internet Settings"), 
	translate("Configurable PPPoE / DHCP / 3g-4g SIM / WiFi client for Internet access."))

s = m:section(TypedSection, "internets")
s.anonymous = true
--s.addremove = true

-----------------------Status--------------------------

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

local protos  = luci.sys.exec("uci get network.@internets[0].proto")
st = s:option(DummyValue, "__statuswifi", translate("Status"))
st:depends({proto="wificlient"})
if string.gsub(protos,"\n","")== "wificlient" then
	st.on_init = set_status("wwan")
else
	st.on_init = set_status("-")
end

st = s:option(DummyValue, "__status4g", translate("Status"))
st:depends({proto="3g"})
if string.gsub(protos,"\n","")== "3g" then
	st.on_init = set_status("4g")
else
	st.on_init = set_status("-")
end

st = s:option(DummyValue, "__statusdhcp", translate("Status"))
st:depends({proto="dhcp"})
if string.gsub(protos,"\n","")== "dhcp" then
	st.on_init = set_status("wan")
else
	st.on_init = set_status("-")
end

st = s:option(DummyValue, "__statusdhcp", translate("Status"))
st:depends({proto="pppoe"})
if string.gsub(protos,"\n","")== "pppoe" then
	
else
	st.on_init = set_status("-")
end


-----------------------List--------------------------

protocol = s:option(ListValue, "proto",
		translate("Protocol"))
protocol.default = "3g"
protocol:value("pppoe", "PPPoE")
protocol:value("dhcp", "DHCP")
protocol:value("3g", "4G")
protocol:value("wificlient", "WIFI Client")


-----------------------PPPoE--------------------------

username = s:option(Value, "username",
		translate("PAP/CHAP username"))
username:depends({proto="pppoe"})

password = s:option(Value, "password",
		translate("PAP/CHAP password"))
password:depends({proto="pppoe"})
password.password = true


-----------------------3G-4G SIM--------------------------

--device = s:option(ListValue, "device",
--		translate("Modem device"))
--luci.sys.exec("find /dev/ -name tty*S* >/tmp/fintty")
--modens= io.open("/tmp/fintty")
--for i,v in modens:lines() do
--	device:value(i, i)
--end
--device.default = "/dev/ttyUSB2"
--device:depends({proto="3g"})

--service= s:option(ListValue, "service",
--		translate("Service Type"))
--service:value("auto", translate("auto"))
--service.default = "auto"
--service:depends({proto="3g"})


apn= s:option(Value, "apn",
		translate("APN"))
apn.default = ""
apn:depends({proto="3g"})

pincode= s:option(Value, "pincode",
		translate("PIN"))
pincode.default = ""
pincode:depends({proto="3g"})

usernameg=s:option(Value, "usernames",
		translate("PAP/CHAP Username"))
usernameg.default = "card"
usernameg:depends({proto="3g"})

passwordg=s:option(Value, "passwords",
		translate("PAP/CHAP Password"))
passwordg.default = "card"
passwordg:depends({proto="3g"})
passwordg.password = true



-----------------------WIFI Client--------------------------

scan_btn = s:option(Button, "", translate("Scan"))
scan_btn.inputtitle = translate("Scan")
scan_btn:depends({proto="wificlient"})

function scan_btn.write()
	--local scan_url = luci.dispatcher.build_url("admin/internetsettings/internet")
	--luci.http.redirect(scan_url)
--local sta_ifname = uci:get("wireless", "wifi-iface")
local iw = luci.sys.wifi.getiwinfo("radio0")
for k, v in ipairs(iw.scanlist or { }) do
	if v.ssid then
		ssid:value(v.ssid, "%s" %{ v.ssid })
	end
end

end

ssid = s:option(Value, "ssid", translate("SSID"))
ssid.validator = 'maxlength="32"'
if scan_start and scan_start == "1" then
local sta_ifname = uci:get("wireless", "wlan0", "ifname")
local iw = luci.sys.wifi.getiwinfo(sta_ifname)
for k, v in ipairs(iw.scanlist or { }) do
	if v.ssid then
		ssid:value(v.ssid, "%s" %{ v.ssid })
	end
end
end
ssid:depends({proto="wificlient"})
ssid.placeholder = translate("SSID is required")
--ssid.rmempty = false

enc = s:option(ListValue, "encryption", translate("Encryption"))
enc:value("none", translate("No Encryption"))
enc:value("psk-mixed", translate("mixed-psk"))
function enc.write(self, section, value)
	luci.sys.exec("uci set wireless.@wifi-iface[1].encryption="..value.."")
	luci.sys.exec("uci set network.@internets[0].encryption="..value.."")
	self.map.uci:commit("network")
	self.map.uci:commit("wireless")

	--self.map.uci:set("wireless", section, "encryption", value)
end
enc:depends({proto="wificlient"})

key = s:option(Value, "key", translate("Key"))
key.datatype = "wpakey"
key.placeholder = translate("Key")
key.password = true
key.rmempty = true
key:depends("encryption","psk-mixed")
key.errmsg = translate("Key can't be empty")
function key.write(self, section, value)
	if value then
		luci.sys.exec("uci set wireless.@wifi-iface[1].key="..value.."")
		luci.sys.exec("uci set network.@internets[0].key="..value.."")
		self.map.uci:commit("network")
		self.map.uci:commit("wireless")
	else
		--luci.sys.exec(" uci delete wireless.@wifi-iface[1].key")
		--self.map.uci:commit("wireless")
	end
	--self.map.uci:set("wireless", "wifi-iface[1]", "key", value)
end

wds = s:option(Flag, "wds", translate("WDS"))
wds.default = "0"
wds.rmempty = false
wds.submit = true
wds:depends({proto="wificlient"})
function wds.write(self, section, value)
	if value=="1" then
		luci.sys.exec("uci set network.@internets[0].wds="..value.."")
		luci.sys.exec("uci set wireless.@wifi-iface[1].wds="..value.."")

	else
		luci.sys.exec("uci set network.@internets[0].wds="..value.."")
		luci.sys.exec("uci delete wireless.@wifi-iface[1].wds")
	end
	self.map.uci:commit("wireless")
	self.map.uci:commit("network")
end


--network = s:option(Value, "network", translate("network"))
--network:value("lan", "lan")
--network:value("wwan0", "wwan0")
--network:depends({proto="wificlient"})

function fork_exec(command)
	local pid = nixio.fork()
	if pid > 0 then
		return
	elseif pid == 0 then
		-- change to root dir
		nixio.chdir("/")

		-- patch stdin, out, err to /dev/null
		local null = nixio.open("/dev/null", "w+")
		if null then
			nixio.dup(null, nixio.stderr)
			nixio.dup(null, nixio.stdout)
			nixio.dup(null, nixio.stdin)
			if null:fileno() > 2 then
				null:close()
			end
		end

		-- replace with target command
		nixio.exec("/bin/sh", "-c", command)
	end
end

function m.on_after_commit(map)
local proto  = luci.sys.exec("uci get network.@internets[0].proto")

-----------------------WIFI Client--------------------------
if string.gsub(proto,"\n","") == "wificlient" then
	local ssid= luci.sys.exec("uci get network.@internets[0].ssid")
	luci.sys.exec("uci set wireless.@wifi-iface[1].disabled='0'")
	luci.sys.exec("uci set wireless.@wifi-iface[1].ssid="..string.gsub(ssid,"\n","").."")
	
else
	luci.sys.exec("uci set wireless.@wifi-iface[1].disabled='1'")
end

-----------------------3g-4g--------------------------
if string.gsub(proto,"\n","") == "3g" then
	local usernames= luci.sys.exec("uci get network.@internets[0].usernames")
	local passwords= luci.sys.exec("uci get network.@internets[0].passwords")
	local pincode= luci.sys.exec("uci get network.@internets[0].pincode")
	local apn= luci.sys.exec("uci get network.@internets[0].apn")
	pincode=string.gsub(pincode,"\n","")
	apn=string.gsub(apn,"\n","")
	if pincode then
		luci.sys.exec("uci set network.4g.pincode="..pincode.."")
	else
		luci.sys.exec("uci delete network.4g.pincode")
	end
	if apn then
		luci.sys.exec("uci set network.4g.apn="..apn.."")
	else
		luci.sys.exec("uci delete network.4g.apn")
	end
	fork_exec("ifdown -w 4g;ifup -w 4g")
else
	fork_exec("ifdown -w 4g")
end


-----------------------dhcp--------------------------

if string.gsub(proto,"\n","") == "dhcp" then
	luci.sys.exec("uci set network.wan.ifname='eth0.2'")
	luci.sys.exec("uci set network.wan._orig_ifname='eth0.2'")
	luci.sys.exec("uci set network.wan._orig_bridge='false'")
	luci.sys.exec("uci set network.wan.proto='dhcp'")
	fork_exec("ifdown -w wan;ifup -w wan")
else
	fork_exec("ifdown -w wan")
end

-----------------------pppoe--------------------------


	map.uci:commit("wireless")
	map.uci:commit("network")
	fork_exec("ifdown -w wwan; wifi up wifi0; ifup -w wwan")	
end

return m
