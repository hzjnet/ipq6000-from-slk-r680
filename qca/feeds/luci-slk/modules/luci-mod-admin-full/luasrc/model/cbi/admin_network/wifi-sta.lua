-- Copyright 2012 Christian Gagneraud <chris@techworks.ie>
-- Licensed to the public under the Apache License 2.0.

--local nw = require "luci.model.network"
--local net = nw:get_network("wlan0")
local nw = require "luci.model.network"
m = Map("wireless")
--[[
	translate("Wireless Settings"), 
	translate("Configurable WiFi client for Internet access."))
--]]
nw.init(m.uci)
local wnet = nw:get_wifinet("radio0.network2")
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
if string.gsub(luci.sys.exec("uci get wireless.@wifi-iface[1]._disabled"),"%s+","") == "1" then
	st.on_init = set_status("wwan")
else
	st.on_init = set_status("-")
end
--if string.gsub(protos,"\n","")=="wificlient" then
--st.on_init = set_status(sts)

--else
	--st.on_init = set_status("-")
	
--end

-----------------------WIFI Client--------------------------
disabled = s:option(Flag, "_disabled", translate("Enable"))
disabled.rmempty = false
--disabled.rmempty = false


scan_btn = s:option(Button, "", translate("Scan"))
scan_btn.inputtitle = translate("Scan")
scan_btn:depends({_disabled="1"})
function scan_btn.write()

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
ssid.placeholder = translate("SSID is required")
ssid:depends({_disabled="1"})
ssid.default="__TEST_AP"
--ssid.rmempty = true

enc = s:option(ListValue, "encryption", translate("Encryption"))
enc:value("none", translate("No Encryption"))
enc:value("psk-mixed", translate("mixed-psk"))
enc:depends({_disabled="1"})

key = s:option(Value, "key", translate("Key"))
key.datatype = "wpakey"
key.placeholder = translate("Key")
key.password = true
--key.rmempty = true
key:depends("encryption","psk-mixed")
--key:depends({_disabled="1"})
key.errmsg = translate("Key can't be empty")
--[[
wds = s:option(Flag, "wds", translate("WDS"))
wds.default = "0"
wds.rmempty = false
wds.submit = true
wds:depends({_disabled="1"})
--]]
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
	map.uci:commit("wireless")
	map.uci:commit("network")
	--fork_exec("ifdown -w wwan; wifi up wifi0; ifup -w wwan")	
end


local apply = luci.http.formvalue("cbi.apply")
if apply then
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

	luci.sys.exec("echo "..wnet.sid.." > /tmp/sta_ifname")
	if luci.http.formvalue("cbid.wireless."..wnet.sid.."._disabled") == "1" then
		luci.sys.exec("uci del wireless.@wifi-iface[1].disabled")
		luci.sys.exec("uci set network.wwan.ifname='wlan0'")
	else
		luci.sys.exec("uci set network.wwan.ifname='-'")
		luci.sys.exec("uci set wireless.@wifi-iface[1].disabled='1'")
	end
	luci.sys.exec("uci commit wireless")
	luci.sys.exec("uci commit network")
end


return m
