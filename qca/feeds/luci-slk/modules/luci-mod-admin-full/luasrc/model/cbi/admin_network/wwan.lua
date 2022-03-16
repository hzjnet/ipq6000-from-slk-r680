local uci = require("luci.model.uci").cursor()
local sys = require "luci.sys"
local os = os
local scan_start = luci.http.formvalue("scan_start")
local w_ifname = "wifi0"
local w5_ifname = "wifi1"
local string = string

wifi0_mode = uci:get("wireless", "wifi0", "def_hwmode")
wifi1_mode = uci:get("wireless", "wifi1", "def_hwmode")

if wifi0_mode == "11ng" or wifi0_mode == "11axg" then
m = Map("wireless", translate("WWAN 2.4G Settings"))
else
m = Map("wireless", translate("WWAN 5G Settings"))
end

s = m:section(NamedSection, "sta0", translate("WWAN0 Settings"))
s.anonymous = true
s.addremove = false

en = s:option(Flag, "disabled", translate("Enable"))
en.enabled="0"
en.disabled="1"
en.default = "1"
en.rmempty = false
en.submit = true

scan_btn = s:option(Button, "", translate("Scan"))
scan_btn.inputtitle = translate("Scan")

function scan_btn.write()
	local scan_url = luci.dispatcher.build_url("admin/network/wwan/scan0")
	luci.http.redirect(scan_url)
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
ssid.rmempty = false

enc = s:option(ListValue, "encryption", translate("Encryption"))
enc:value("none", translate("No Encryption"))
enc:value("mixed-psk", translate("mixed-psk"))
function enc.write(self, section, value)
	self.map.uci:set("wireless", section, "encryption", value)
end

key = s:option(Value, "key", translate("Key"))
key.datatype = "wpakey"
key.placeholder = translate("Key")
key.password = true
key:depends("encryption","mixed-psk")
key.errmsg = translate("Key can't be empty")
function key.write(self, section, value)
	self.map.uci:set("wireless", section, "key", value)
end

wds = s:option(Flag, "wds", translate("WDS"))
wds.default = "0"
wds.rmempty = false
wds.submit = true

network = s:option(Value, "network", translate("network"))
network:value("lan", "lan")
network:value("wwan0", "wwan0")

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
	if map.changed == true then
		local sta0_net = map.uci:get("wireless", "sta0", "network")
		local sta0_disabled = map.uci:get("wireless", "sta0", "disabled")
		local sta0_ifname = map.uci:get("wireless", "sta0", "ifname")


		if sta0_disabled ~= nil and sta0_disabled == "1" then
			map.uci:set("network", "wwan0", "disabled", 1)
			map.uci:commit("network")
		else
			if sta0_net == "wwan0" then
				map.uci:set("network", "wwan0", "disabled", 0)
				map.uci:set("network", "wwan0", "ifname", sta0_ifname)
			else
				map.uci:set("network", "wwan0", "disabled", 1)
			end

			map.uci:commit("network")
		end

		fork_exec("ifdown -w wwan0; wifi up wifi0; ifup -w wwan0")
	end
end

if wifi1_mode == "11ng" or wifi1_mode == "11axg" then
m2 = Map("wireless", translate("WWAN 2.4G Settings"))
else
m2 = Map("wireless", translate("WWAN 5G Settings"))
end

s1 = m2:section(NamedSection, "sta1", translate("WWAN1 Settings"))
s1.anonymous = true
s1.addremove = false

en1 = s1:option(Flag, "disabled", translate("Enable"))
en1.enabled="0"
en1.disabled="1"
en1.default = "1"
en1.rmempty = false
en1.submit = true

scan1_btn = s1:option(Button, "", translate("Scan"))
scan1_btn.inputtitle = translate("Scan")

function scan1_btn.write()
	local scan_url = luci.dispatcher.build_url("admin/network/wwan/scan1")
	luci.http.redirect(scan_url)
end

ssid1 = s1:option(Value, "ssid", translate("SSID"))
ssid1.validator = 'maxlength="32"'
if scan_start and scan_start == "2" then
local sta_ifname = uci:get("wireless", "wlan1", "ifname")
local iw1 = luci.sys.wifi.getiwinfo(sta_ifname)
for k, v in ipairs(iw1.scanlist or { }) do
	if v.ssid then
		ssid1:value(v.ssid, "%s" %{ v.ssid })
	end
end
end
ssid1.placeholder = translate("SSID is required")
ssid1.rmempty = false

enc1 = s1:option(ListValue, "encryption", translate("Encryption"))
enc1:value("none", translate("No Encryption"))
enc1:value("mixed-psk", translate("mixed-psk"))
function enc1.write(self, section, value)
	self.map.uci:set("wireless", section, "encryption", value)
end

key1 = s1:option(Value, "key", translate("Key"))
key1.datatype = "wpakey"
key1.placeholder = translate("Key")
key1.password = true
key1:depends("encryption","mixed-psk")
key1.errmsg = translate("Key can't be empty")
function key1.write(self, section, value)
	self.map.uci:set("wireless", section, "key", value)
end

wds1 = s1:option(Flag, "wds", translate("WDS"))
wds1.default = "0"
wds1.rmempty = false
wds1.submit = true

network1 = s1:option(Value, "network", translate("network"))
network1:value("lan", "lan")
network1:value("wwan1", "wwan1")

function m2.on_after_commit(map)
	if map.changed == true then
		local sta1_net = map.uci:get("wireless", "sta1", "network")
		local sta1_disabled = map.uci:get("wireless", "sta1", "disabled")
		local sta1_ifname = map.uci:get("wireless", "sta1", "ifname")

		if sta1_disabled ~= nil and sta1_disabled == "1" then
			map.uci:set("network", "wwan1", "disabled", 1)
			map.uci:commit("network")
		else
			if sta1_net == "wwan1" then
				map.uci:set("network", "wwan1", "disabled", 0)
				map.uci:set("network", "wwan1", "ifname", sta1_ifname)
			else
				map.uci:set("network", "wwan1", "disabled", 1)
			end

			map.uci:commit("network")
		end

		fork_exec("ifdown -w wwan1; wifi up wifi1; ifup -w wwan1")
	end
end

return m,m2
