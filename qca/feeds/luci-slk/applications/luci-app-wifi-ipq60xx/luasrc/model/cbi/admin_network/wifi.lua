-- Copyright 2012 Christian Gagneraud <chris@techworks.ie>
-- Licensed to the public under the Apache License 2.0.


--[[------------------------- WIFI 0 -------------------------]]--
local nw = require "luci.model.network"

local isARV7name={"2.4G","5.8G"}

local isARV7ipq6000=string.gsub(luci.sys.exec("cat /proc/cpuinfo | grep 'Hardware'| sed -n '1p' | awk -F ':' '{print $2}'| awk -F ' ' '{print $4}'"),"%s+","")
if isARV7ipq6000 == "IPQ6010" then
	isARV7name={"5.8G","2.4G"}
end

m = Map("wireless"," ",translate("Configure "..isARV7name[1].." WiFi"))
	--translate("Wireless Settings"))

nw.init(m.uci)
local wnet = nw:get_wifinet("wifi0.network1")
local wdev = wnet and wnet:get_device()
local iw = luci.sys.wifi.getiwinfo("wifi0.network1")
local iw2 = luci.sys.wifi.getiwinfo("wifi1.network1")
s = m:section(NamedSection,wnet.sid,"wifi-iface")
s.anonymous = true
--s.addremove = true

function s.filter(self, section)
	return m.uci:get("wireless", section, "mode") == "ap"
end

-----------------------Status--------------------------

local protos  = luci.sys.exec("uci get network.@internets[0].proto")
st = s:option(DummyValue, "__statuswifi", translate("Status"))

st.template = "admin_network/wifi_status"
st.ifname   = "wifi0.network1"
--if string.gsub(luci.sys.exec("uci get wireless.wlan0.disabled"),"%s+","") == "1" then
--	st.ifname   = "-"
--else
--	st.ifname   = "ath0"
--end



------------------------Disable or Enable-------------------------
en = s:option(Button, "__toggle")

--local disableds=string.gsub(luci.sys.exec("uci get wireless.@wifi-iface[1].disabled"),"\n","")
if wdev:get("disabled") == "1" or wnet:get("disabled") == "1" then
	en.title      = translate("Wireless network is disabled ")
	en.inputtitle = translate("Enable")
	en.inputstyle = "apply"
else
	en.title      = translate("Wireless network is enabled ")
	en.inputtitle = translate("Disable")
	en.inputstyle = "reset"
end

function en.parse()
	if m:formvalue("cbid.wireless.%s.__toggle" % wnet.sid) then
		if wdev:get("disabled") == "1" or wnet:get("disabled") == "1" then
			wnet:set("disabled", nil)
		else
			wnet:set("disabled", "1")
		end
		wdev:set("disabled", nil)
		
		nw:commit("wireless")
		--luci.sys.exec("(env -i /bin/ubus call network reload) >/dev/null 2>/dev/null")
		luci.sys.exec("wifi up wifi0 &")
		luci.http.redirect(luci.dispatcher.build_url("admin/network/wifiap"))
		return
	end

end
------------------------ESSID-------------------------

--s = m:section(NamdSection, wnet.sid, "wifi-iface")
ssid=s:option(Value, "ssid", translate("<abbr title=\"Extended Service Set Identifier\">ESSID</abbr>"))
mac=string.gsub(luci.sys.exec("cat /sys/class/net/wifi0/address|awk -F ':' '{print $4''$5''$6 }'| tr a-z A-Z"),"%s+","")
ssid.default="SLK-Routers_"..mac..""
ssid.placeholder="SLK-Routers_"..mac..""
--[[
function ssid.on_init()
local ssidcode=string.gsub(luci.sys.exec("uci get wireless.@wifi-iface[0].ssid"),"\n","")
ssid.default=ssidcode
end
--]]
--ssid.rmempty = false
------------------------Hidden-------------------------

hidden=s:option(Flag, "hidden", translate("Hide <abbr title=\"Extended Service Set Identifier\">ESSID</abbr>"))


------------------------Encryption-------------------------

encryption=s:option(ListValue, "encryption", translate("Encryption"))
encryption:value("none","No Encryption")
--encryption:value("psk","WPA-PSK")
--encryption:value("psk2","WPA2-PSK")
encryption:value("mixed-psk","WPA-PSK/WPA2-PSK Mixed Mode")
--[[
function hidden.on_init()
local encryptioncode=string.gsub(luci.sys.exec("uci get wireless.@wifi-iface[0].encryption"),"\n","")
encryption.default=encryptioncode
end
encryption.rmempty = false
--]]

------------------------Key-------------------------

key=s:option(Value, "key", translate("Key"))
key.password=true
key.default="slk100200"
key:depends({encryption="psk"})
key:depends({encryption="psk2"})
key:depends({encryption="mixed-psk"})
--key.rmempty = false

--s = m:section(NamedSection,wdev:name(),"wifi-iface")
--s.anonymous = true


------------------------channel-------------------------
channel=s:option(ListValue, "channel", translate("Channel"))
local channeld=string.gsub(luci.sys.exec("uci get wireless."..wdev:name()..".channel"),"\n","")
channel.default=channeld
channel:value("auto",translate("auto"))
		for _, f in ipairs(iw and iw.freqlist or { }) do
			if not f.restricted then
				channel:value(f.channel, "%i (%.3f GHz)" %{ f.channel, f.mhz / 1000 })
			end
		end
--[[
for i=1,13 do
	channel:value(i,i)
end
--]]
------------------------htmode-------------------------
if isARV7ipq6000 ~= "IPQ6000" then
htmode=s:option(ListValue, "htmode", translate("Width"))
local htmoded=string.gsub(luci.sys.exec("uci get wireless."..wdev:name()..".htmode"),"\n","")
htmode.default=htmoded
htmode:value("HT20","HT20")
htmode:value("HT40","HT40")
end



--[[------------------------- WIFI 1 -------------------------]]--
local nw2 = require "luci.model.network"



m2 = Map("wireless",nil,translate("Configure "..isARV7name[2].." WiFi"))
	--translate("Wireless Settings"))

nw2.init(m.uci)
local wnet2 = nw2:get_wifinet("wifi1.network1")
local wdev2 = wnet2 and wnet2:get_device()

s = m2:section(NamedSection,wnet2.sid,"wifi-iface")
s.anonymous = true
--s.addremove = true



-----------------------Status--------------------------

st = s:option(DummyValue, "__statuswifi", translate("Status"))

st.template = "admin_network/wifi_status"
st.ifname   = "wifi1.network1"--"ath1"


------------------------Disable or Enable-------------------------
en2 = s:option(Button, "__toggle")

--local disableds=string.gsub(luci.sys.exec("uci get wireless.@wifi-iface[1].disabled"),"\n","")
if wdev2:get("disabled") == "1" or wnet2:get("disabled") == "1" then
	en2.title      = translate("Wireless network is disabled ")
	en2.inputtitle = translate("Enable")
	en2.inputstyle = "apply"
else
	en2.title      = translate("Wireless network is enabled ")
	en2.inputtitle = translate("Disable")
	en2.inputstyle = "reset"
end

function en2.parse()
	if m2:formvalue("cbid.wireless.%s.__toggle" % wnet2.sid) then
		if wdev2:get("disabled") == "1" or wnet2:get("disabled") == "1" then
			wnet2:set("disabled", nil)
		else
			wnet2:set("disabled", "1")
		end
		wdev2:set("disabled", nil)
		
		nw2:commit("wireless")
		--luci.sys.exec("(env -i /bin/ubus call network reload) >/dev/null 2>/dev/null")
		luci.sys.exec("wifi up wifi1 &")
		luci.http.redirect(luci.dispatcher.build_url("admin/network/wifiap"))
		return 
	end

end
------------------------ESSID-------------------------

--s = m:section(NamdSection, wnet.sid, "wifi-iface")
ssid2=s:option(Value, "ssid", translate("<abbr title=\"Extended Service Set Identifier\">ESSID</abbr>"))
mac2=string.gsub(luci.sys.exec("cat /sys/class/net/wifi1/address|awk -F ':' '{print $4''$5''$6 }'| tr a-z A-Z"),"%s+","")
ssid2.default="SLK-Routers_5G-"..mac2..""
ssid2.placeholder="SLK-Routers_5G-"..mac2..""
--ssid2.rmempty = false
------------------------Hidden-------------------------

hidden2=s:option(Flag, "hidden", translate("Hide <abbr title=\"Extended Service Set Identifier\">ESSID</abbr>"))
--[[
function hidden2.on_init()
local hiddencode=string.gsub(luci.sys.exec("uci get wireless.@wifi-iface[0].hidden"),"\n","")
hidden2.default=hiddencode
end
--]]
------------------------Encryption-------------------------

encryption2=s:option(ListValue, "encryption", translate("Encryption"))
encryption2:value("none","No Encryption")
--encryption2:value("psk","WPA-PSK")
--encryption2:value("psk2","WPA2-PSK")
encryption2:value("mixed-psk","WPA-PSK/WPA2-PSK Mixed Mode")
--[[
function encryption.on_init()
local encryptioncode=string.gsub(luci.sys.exec("uci get wireless.@wifi-iface[0].encryption"),"\n","")
encryption.default=encryptioncode
end
--encryption.rmempty = false
--]]

------------------------Key-------------------------

key=s:option(Value, "key", translate("Key"))
key.password=true
key.default="slk100200"
key:depends({encryption="psk"})
key:depends({encryption="psk2"})
key:depends({encryption="mixed-psk"})
--key.rmempty = false

--s = m:section(NamedSection,wdev:name(),"wifi-iface")
--s.anonymous = true
--country2=s:option(ListValue, "country", translate("Country"))
--[[
	local cl = iw2 and iw2.countrylist
	if cl and #cl > 0 then
		cc = s:option(ListValue, "country", translate("Country Code"), translate("Use ISO/IEC 3166 alpha2 country codes."))
		cc.default = tostring(iw2 and iw2.country or "00")
		for _, c in ipairs(cl) do
			cc:value(c.alpha2, "%s - %s" %{ c.alpha2, c.name })
		end
	else
		s:option(Value, "country", translate("Country Code"), translate("Use ISO/IEC 3166 alpha2 country codes."))
	end
--]]
------------------------channel-------------------------
channel2=s:option(ListValue, "channel", translate("Channel"))
local channeld2=string.gsub(luci.sys.exec("uci get wireless."..wdev2:name()..".channel"),"\n","")
channel2.default=channeld2
channel2:value("auto",translate("auto"))
		for _, f2 in ipairs(iw2 and iw2.freqlist or { }) do
			if not f2.restricted then
				channel2:value(f2.channel, "%i (%.3f GHz)" %{ f2.channel, f2.mhz / 1000 })
			end
		end


------------------------htmode-------------------------
if isARV7ipq6000 ~= "IPQ6000" then
htmode=s:option(ListValue, "htmode", translate("Width"))
local htmoded=string.gsub(luci.sys.exec("uci get wireless."..wdev2:name()..".htmode"),"\n","")
htmode.default=htmoded
htmode:value("HT20","HT20")
htmode:value("HT40","HT40")
htmode:value("HT80","HT80")
--htmode:value("VHT160","HT160")
end

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


--function m.on_after_commit(map)
local apply = luci.http.formvalue("cbi.apply")
if apply then
	local names=wdev:name()
	local names2=wdev2:name()
	local channels=luci.http.formvalue("cbid.wireless.%s.channel" % wnet.sid)
	local htmodes=luci.http.formvalue("cbid.wireless.%s.htmode" % wnet.sid)
	local channels5G=luci.http.formvalue("cbid.wireless.%s.channel" % wnet2.sid)
	local htmodes5G=luci.http.formvalue("cbid.wireless.%s.htmode" % wnet2.sid)
	
	if channels then
		luci.sys.exec("uci set wireless."..wdev:name()..".channel="..channels.."")
		luci.sys.exec("uci del wireless."..wnet.sid..".channel")
	end
	if htmodes then
		luci.sys.exec("uci set wireless."..wdev:name()..".htmode="..htmodes.."")
		luci.sys.exec("uci del wireless."..wnet.sid..".htmode")
	end
	if channels5G then
		luci.sys.exec("uci set wireless."..wdev2:name()..".channel="..channels5G.."")
		luci.sys.exec("uci del wireless."..wnet2.sid..".channel")
	end
	if htmodes5G then
		luci.sys.exec("uci set wireless."..wdev2:name()..".htmode="..htmodes5G.."")
		luci.sys.exec("uci del wireless."..wnet2.sid..".htmode")
	end
	luci.sys.exec("uci commit wireless")

	--fork_exec("wifi up wifi1")
--[[
	local ssids=luci.http.formvalue("cbid.wireless.%s.ssid" % wnet.sid)
	local hiddens=luci.http.formvalue("cbid.wireless.%s.hidden" % wnet.sid)
	local encryptions=luci.http.formvalue("cbid.wireless.%s.encryption" % wnet.sid)
	local keys=luci.http.formvalue("cbid.wireless.%s.key" % wnet.sid)
	local channels=luci.http.formvalue("cbid.wireless.%s.channel" % wnet.sid)
	local htmodes=luci.http.formvalue("cbid.wireless.%s.htmode" % wnet.sid)


	if ssids == nil then
		luci.sys.exec("uci set wireless.@wifi-iface[0].ssid="..string.gsub(luci.sys.exec("uci get wireless.@wifi-iface[0].ssid"),"\n","").."")
	else
		luci.sys.exec("uci set wireless.@wifi-iface[0].ssid="..ssids.."")
	end

	if hiddens then
		luci.sys.exec("uci set wireless.@wifi-iface[0].hidden='1'")
	else
		luci.sys.exec("uci delete wireless.@wifi-iface[0].hidden")
	end
	if encryptions then
		luci.sys.exec("uci set wireless.@wifi-iface[0].encryption="..encryptions.."")
	end
	if keys == nil then
		luci.sys.exec("uci delete wireless.@wifi-iface[0].key")
	else
		luci.sys.exec("uci set wireless.@wifi-iface[0].key="..keys.."")
	end
	if channels then
		luci.sys.exec("uci set wireless.radio0.channel="..channels.."")
	end
	if htmodes then
		luci.sys.exec("uci set wireless.radio0.htmode="..htmodes.."")
	end

	luci.sys.exec("uci commit wireless")
	fork_exec("wifi up wifi1")
	--luci.http.redirect(luci.dispatcher.build_url("admin/network/wifi"))
	--return
--]]
end
--end

return m,m2
