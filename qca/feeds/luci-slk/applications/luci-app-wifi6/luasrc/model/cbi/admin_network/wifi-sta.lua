-- Copyright 2012 Christian Gagneraud <chris@techworks.ie>
-- Licensed to the public under the Apache License 2.0.

--local nw = require "luci.model.network"
--local net = nw:get_network("wlan0")
local wifiinfo=string.gsub(luci.sys.exec("uci get wireless.sta0.device"),"%s+","")
--[[
if wifiinfo == nil or wifiinfo == "" then
else
	if string.gsub(luci.sys.exec("uci get wireless.sta0.ifname"),"%s+","") == "ath01" then
	luci.sys.exec("uci set wireless.sta0.device='wifi1'")
	else
	luci.sys.exec("uci set wireless.sta0.device='wifi0'")
	end
	luci.sys.exec("uci commit wireless")
		wifiinfo=string.gsub(luci.sys.exec("uci get wireless.sta0.device"),"%s+","")
end
--]]
	if string.gsub(luci.sys.exec("uci get wireless.sta0.ifname"),"%s+","") == "ath01" then
		luci.sys.exec("uci set wireless.sta0.device='wifi1'")
	else
		luci.sys.exec("uci set wireless.sta0.device='wifi0'")
	end
	luci.sys.exec("uci commit wireless")
	wifiinfo=string.gsub(luci.sys.exec("uci get wireless.sta0.device"),"%s+","")

local nw = require "luci.model.network"
m = Map("wireless",translate("General Setup"))
--[[
	translate("Wireless Settings"), 
	translate("Configurable WiFi client for Internet access."))
--]]
nw.init(m.uci)

local wnet = nw:get_wifinet(""..wifiinfo..".network2")
local wdev = wnet and wnet:get_device()

s = m:section(NamedSection,wnet.sid,"wifi-iface")
s.anonymous = true
--s.addremove = true

function s.filter(self, section)
	return m.uci:get("wireless", section, "mode") == "sta"
end

-----------------------Status--------------------------

local function set_status(self)
	st.template = "admin_network/iface_status"
	st.network  = self
	st.value    = nil
end


--local protos  = luci.sys.exec("uci get network.@internets[0].proto")
st = s:option(DummyValue, "__statuswifi", translate("Status"))
--st.template="admin_network/wifi_status"
--st.ifname="ath01"
	st.on_init = set_status("wific")
	--[[
if string.gsub(luci.sys.exec("uci get wireless.@wifi-iface[1]._disabled"),"%s+","") == "1" then

else
	st.on_init = set_status("-")
end--]]
--if string.gsub(protos,"\n","")=="wificlient" then
--st.on_init = set_status(sts)

--else
	--st.on_init = set_status("-")
	
--end


-----------------------WIFI Client--------------------------
disabled = s:option(Flag, "_disabled", translate("Enable"))
disabled.rmempty = false
--disabled.rmempty = false

infos = s:option(ListValue, "device", translate("WiFi Interface"))
infos.default=wifiinfo--string.gsub(luci.sys.exec("uci set wireless."..wnet.sid..".device"),"%s+","")
--infos:depends({_disabled="1"})
infos:value("wifi1",translate("2.4G Client"))
infos:value("wifi0",translate("5.8G Client"))
--infos.rmempty = false
--[[
function infos.parse()
	if m:formvalue("cbid.wireless.%s.__wifiinfos" % wnet.sid) then
		local one = m:formvalue("cbid.wireless.%s.__wifiinfos" % wnet.sid)
		luci.sys.exec("uci set system.@system[0].wifiinfos="..one.."")
		wnet:set("device", one)--luci.sys.exec("uci set wireless."..wnet.sid..".device="..one.."")
	end
		nw:commit("wireless")
		luci.sys.exec("uci commit system")
		--luci.http.redirect(luci.dispatcher.build_url("admin/network/wifi/client"))
	--return
end
--]]
scan_btn = s:option(Button, "", translate("Scan"))
scan_btn.inputtitle = translate("Scan")
scan_btn:depends({_disabled="1"})
--scan_btn:depends({device="wifi0"})
local one= "wifi0";
function scan_btn.write()
one = m:formvalue("cbid.wireless.%s.device" % wnet.sid)
if one == "wifi0" then
	one="ath0"
else
	one="ath1"
end
luci.sys.exec("sleep 1s;")
local iw = luci.sys.wifi.getiwinfo(one)

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
ssid.placeholder = translate("Enter or click search")--translate("SSID is required")
ssid:depends({_disabled="1"})
ssid.default="__TEST_AP"
--ssid.placeholder = translate("WIFI")
--ssid.rmempty = true

enc = s:option(ListValue, "encryption", translate("Security"))
enc:value("none", translate("None"))
enc:value("mixed-psk", translate("Encryption"))
enc:depends({_disabled="1"})

key = s:option(Value, "key", translate("Key"))
key.datatype = "wpakey"
key.placeholder = translate("Key")
key.password = true
--key.rmempty = true
key:depends("encryption","mixed-psk")
--key:depends({_disabled="1"})
key.errmsg = translate("Key can't be empty")

wds = s:option(Flag, "wds", translate("WDS"))
wds.default = "0"
wds.rmempty = false
wds.submit = true
wds:depends({_disabled="1"})

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
--[[
function m.on_after_commit(map)
	map.uci:commit("wireless")
	map.uci:commit("network")
	--fork_exec("ifdown -w wwan; wifi up wifi0; ifup -w wwan")	
end
--]]


local sys   = require "luci.sys"
local s, m2 ,s2

m2 = Map("network", translate("Advanced Settings"))
m2:chain("luci")

s2 = m2:section(NamedSection, "wific", "interface")
s2.anonymous = true
s2.addremove = false

protos = s2:option(ListValue, "protos", translate("Protocol"), translate("LAN port of bridge should share the same network segment with upstream equipment"))
protos:value("dhcp", translate("DHCP address"))
protos:value("static", translate("Static address"))
protos:value("bridgelan", translate("Bridge Lan"))

ipaddr = s2:option(Value, "ipaddr", translate("IP Address"))
ipaddr.datatype = "ip4addr"
ipaddr.default = "192.168.1.100"
ipaddr:depends({protos="static"})

netmask = s2:option(Value, "netmask",translate("Netmask"))
netmask.datatype = "ip4addr"
netmask.default = "255.255.255.0"
netmask:value("255.255.255.0")
netmask:value("255.255.0.0")
netmask:value("255.0.0.0")
netmask:depends({protos="static"})

gateway= s2:option(Value, "gateway", translate("Gateway"))
gateway.datatype = "ip4addr"
gateway:depends({protos="static"})

dns= s2:option(DynamicList, "dns", translate("DNS"))
dns.datatype = "ip4addr"
dns.cast     = "string"
dns:depends({protos="static"})



local apply = luci.http.formvalue("cbi.apply")
if apply then
luci.sys.exec("uci set wireless.sta0.mode='sta")
local protos = string.gsub(luci.http.formvalue("cbid.network.wific.protos"),"%s+","")
if protos == "bridgelan" then
	luci.sys.exec("uci set network.wific.proto='dhcp'");
	luci.sys.exec("uci set wireless.sta0.network='lan'");
	luci.sys.exec("uci set wireless.sta0.extap='1'");
	luci.sys.exec("uci set wireless.sta0.vap_ind='1'");
else
	luci.sys.exec("uci del wireless.sta0.extap");
	luci.sys.exec("uci del wireless.sta0.vap_ind");
	luci.sys.exec("uci set network.wific.proto="..protos.."");
	luci.sys.exec("uci set wireless.sta0.network='wific'");
end
--[[
	function disabled.cfgvalue(self,value)
		if luci.http.formvalue("cbid.wireless."..value..".disabled") then
			luci.sys.exec("uci set network.wwan.ifname='-'")
			luci.sys.exec("uci set wireless.@wifi-iface.disabled='1'")
		else
			luci.sys.exec("uci set network.wwan.ifname='wlan0'")
			luci.sys.exec("uci del wireless.@wifi-iface.disabled")
		end
		--luci.sys.exec("echo "..value.." > /tmp/sta_ifname")
		luci.sys.exec("uci commit wireless")
		luci.sys.exec("uci commit network")
	end
--]]
if luci.http.formvalue("cbid.wireless."..wnet.sid..".device") == nil or luci.http.formvalue("cbid.wireless."..wnet.sid..".device") == "" then
	if string.gsub(luci.sys.exec("uci get wireless.sta0.ifname"),"%s+","") == "ath01" then
		luci.sys.exec("uci set wireless."..wnet.sid..".device='wifi1'")
	else
		luci.sys.exec("uci set wireless."..wnet.sid..".device='wifi0'")
	end
end
	luci.sys.exec("echo "..wnet.sid.." > /tmp/sta_ifname")
	local one = "wifi1";
	if luci.http.formvalue("cbid.wireless."..wnet.sid.."._disabled") == "1" then
		luci.sys.exec("uci del wireless."..wnet.sid..".disabled")
		one = m:formvalue("cbid.wireless.%s.device" % wnet.sid)
		if one == "wifi1" then
			one="ath0"
		else
			one="ath1"
		end
		luci.sys.exec("uci set network.wific.ifname='"..one.."1'")
		luci.sys.exec("uci set wireless."..wnet.sid..".ifname='"..one.."1'")
	else
		luci.sys.exec("uci set network.wific.ifname='-'")
		luci.sys.exec("uci set wireless."..wnet.sid..".disabled='1'")
	end
	luci.sys.exec("uci commit wireless")
	luci.sys.exec("uci commit network")
	if one == "wifi1" then
		one = "wifi0"
	else
		one = "wifi1"
	end
	--fork_exec("wifi up "..one.."")
	luci.sys.exec("wifi up "..one.."")
	--luci.sys.exec("/etc/init.d/network restart")
	--fork_exec("ifup -w wific")
	--fork_exec("wifi up "..m:formvalue("cbid.wireless.%s.device" % wnet.sid).."")
end


return m,m2
