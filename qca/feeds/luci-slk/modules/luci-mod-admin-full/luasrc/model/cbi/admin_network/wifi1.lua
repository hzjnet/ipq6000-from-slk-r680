-- Copyright 2012 Christian Gagneraud <chris@techworks.ie>
-- Licensed to the public under the Apache License 2.0.
local nw = require "luci.model.network"



m = Map("wireless")
	--translate("Wireless Settings"))

nw.init(m.uci)
local wnet = nw:get_wifinet("radio0.network1")
local wdev = wnet and wnet:get_device()

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
if string.gsub(luci.sys.exec("uci get wireless.@wifi-iface[1].disabled"),"%s+","") == "1" then
	st.ifname   = "wlan0"
else
	st.ifname   = "wlan0-1"
end

function m.parse()
	if m:formvalue("cbid.wireless.%s.__toggle" % wnet.sid) then
		if wdev:get("disabled") == "1" or wnet:get("disabled") == "1" then
			wnet:set("disabled", nil)
		else
			wnet:set("disabled", "1")
		end
		wdev:set("disabled", nil)
		
		nw:commit("wireless")
		luci.sys.exec("(env -i /bin/ubus call network reload) >/dev/null 2>/dev/null")
		luci.sys.exec("wifi up")
		luci.http.redirect(luci.dispatcher.build_url("admin/network/wifi"))
		return
	end

end

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
------------------------ESSID-------------------------

--s = m:section(NamdSection, wnet.sid, "wifi-iface")
ssid=s:option(Value, "ssid", translate("<abbr title=\"Extended Service Set Identifier\">ESSID</abbr>"))
function ssid.on_init()
local ssidcode=string.gsub(luci.sys.exec("uci get wireless.@wifi-iface[0].ssid"),"\n","")
ssid.default=ssidcode
end
ssid.rmempty = false
------------------------Hidden-------------------------

hidden=s:option(Flag, "hidden", translate("Hide <abbr title=\"Extended Service Set Identifier\">ESSID</abbr>"))
function hidden.on_init()
local hiddencode=string.gsub(luci.sys.exec("uci get wireless.@wifi-iface[0].hidden"),"\n","")
hidden.default=hiddencode
end
ssid.rmempty = false

------------------------Encryption-------------------------

encryption=s:option(ListValue, "encryption", translate("Encryption"))
encryption:value("none","No Encryption")
encryption:value("psk","WPA-PSK")
encryption:value("psk2","WPA2-PSK")
encryption:value("psk-mixed","WPA-PSK/WPA2-PSK Mixed Mode")
function hidden.on_init()
local encryptioncode=string.gsub(luci.sys.exec("uci get wireless.@wifi-iface[0].encryption"),"\n","")
encryption.default=encryptioncode
end
encryption.rmempty = false


------------------------Key-------------------------

key=s:option(Value, "key", translate("Key"))
key.password=true
key:depends({encryption="psk"})
key:depends({encryption="psk2"})
key:depends({encryption="psk-mixed"})
key.rmempty = false

--s = m:section(NamedSection,wdev:name(),"wifi-iface")
--s.anonymous = true


------------------------channel-------------------------
channel=s:option(ListValue, "channel", translate("Channel"))
local channeld=string.gsub(luci.sys.exec("uci get wireless."..wdev:name()..".channel"),"\n","")
channel.default=channeld
channel:value("0","Auto")
for i=1,13 do
	channel:value(i,i)
end

------------------------htmode-------------------------
htmode=s:option(ListValue, "htmode", translate("Width"))
local htmoded=string.gsub(luci.sys.exec("uci get wireless."..wdev:name()..".htmode"),"\n","")
htmode.default=htmoded
htmode:value("HT20","HT20")
htmode:value("HT40","HT40")


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

end
--end

return m
