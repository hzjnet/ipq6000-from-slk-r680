
local uci = require "luci.model.uci".cursor()
wifi0_mode = uci:get("wireless", "wifi0", "def_hwmode")
wifi1_mode = uci:get("wireless", "wifi1", "def_hwmode")

if wifi0_mode == "11ng" or wifi0_mode == "11axg" then
m = Map("wireless", translate("Wireless 2.4G Settings"))
-- 2G
s2 = m:section(NamedSection, "wlan0", translate("Wireless 2.4G Settings"))
else
m = Map("wireless", translate("Wireless 5G Settings"))
-- 2G
s2 = m:section(NamedSection, "wlan0", translate("Wireless 5G Settings"))
end

s2.anonymous = true
s2.addremove = false

s2:tab("wireless2", translate("General Settings"))
en2 = s2:taboption("wireless2", Flag, "disabled", translate("Enable"))
en2.submit = true
en2.rmempty = false

function en2.cfgvalue(...)
	local v = Flag.cfgvalue(...)
	return v == "1" and "0" or "1"
end

function en2.write(self, section, value)
	Flag.write(self, section, value == "1" and "" or "1")
end

ssid2 = s2:taboption("wireless2", Value, "ssid", translate("SSID"))
ssid2.validator = 'maxlength="32"'
ssid2.placeholder = translate("SSID is required")
ssid2.rmempty = false

enc2 = s2:taboption("wireless2", ListValue, "encryption", translate("Encryption"))
enc2:value("none", translate("No Encryption"))
enc2:value("mixed-psk", translate("mixed-psk"))
function enc2.write(self, section, value)
	self.map.uci:set("wireless", section, "encryption", value)
end

key2 = s2:taboption("wireless2", Value, "key", translate("Key"))
key2.datatype = "wpakey"
key2.placeholder = translate("Key")
key2.password = true
key2:depends("encryption","mixed-psk")
key2.errmsg = translate("Key can't be empty")
function key2.write(self, section, value)
	self.map.uci:set("wireless", section, "key", value)
end

hidden2 = s2:taboption("wireless2", Flag, "hidden", translate("Hide SSID"))

sr2 = m:section(NamedSection, "wifi0", translate("Wireless 5G Settings"))
sr2.anonymous = true

sr2:tab("wireless2_radio", translate("Advanced"))

hw2 = sr2:taboption("wireless2_radio", ListValue, "hwmode", translate("HW Mode"))

if wifi0_mode == "11ng" or wifi0_mode == "11axg" then
	if wifi0_mode == "11axg" then
		hw2:value("11axg", "11axg")
	end

	hw2:value("11ng", "11ng")
	hw2:value("11g", "11g")
	hw2:value("11b", "11b")
else
	if wifi0_mode == "11axa" then
		hw2:value("11axa", "11axa")
	end

	hw2:value("11ac", "11ac")
	hw2:value("11na", "11na")
	hw2:value("11a", "11a")
end

local iw2 = luci.sys.wifi.getiwinfo("wifi0")

ch = sr2:taboption("wireless2_radio", ListValue, "channel", translate("Channel"))
ch:value("auto", translate("auto"))
for _, freq in ipairs(iw2.freqlist) do
	ch:value(freq.channel, freq.channel .. " (%d MHz)" % freq.mhz)
end

ht2 = sr2:taboption("wireless2_radio", ListValue, "htmode", translate("HT Mode"))

ht2:value("auto", "auto")
if wifi0_mode == "11axa" or wifi0_mode == "11ac" then
	ht2:value("HT80", "HT80")
end
ht2:value("HT40", "HT40")
ht2:value("HT20", "HT20")

local cl2 = iw2 and iw2.countrylist
if cl2 and #cl2 > 0 then
	cc = sr2:taboption("wireless2_radio", ListValue, "country", translate("Country Code"), translate("Use ISO/IEC 3166 alpha2 country codes."))
	cc.default = tostring(iw2 and iw2.country or "00")
	for _, c in ipairs(cl2) do
		cc:value(c.alpha2, "%s - %s" %{ c.alpha2, c.name })
	end
else
	sr2:taboption("wireless2_radio", Value, "country", translate("Country Code"), translate("Use ISO/IEC 3166 alpha2 country codes."))
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

function m.on_commit(map)
	if map.changed == true then
		fork_exec("wifi up wifi0")
	end
end

-- 5G
if wifi1_mode == "11ng" or wifi1_mode == "11axg" then
m2 = Map("wireless", translate("Wireless 2.4G Settings"))
s5 = m2:section(NamedSection, "wlan1", translate("Wireless 2.4G Settings"))
else
m2 = Map("wireless", translate("Wireless 5G Settings"))
s5 = m2:section(NamedSection, "wlan1", translate("Wireless 5G Settings"))
end

s5.anonymous = true
s5.addremove = false

s5:tab("wireless5", translate("General Settings"))
en5 = s5:taboption("wireless5", Flag, "disabled", translate("Enable"))
en5.submit = true
en5.rmempty = false

function en5.cfgvalue(...)
	local v = Flag.cfgvalue(...)
	return v == "1" and "0" or "1"
end

function en5.write(self, section, value)
	Flag.write(self, section, value == "1" and "" or "1")
end

ssid5 = s5:taboption("wireless5", Value, "ssid", translate("SSID"))
ssid5.validator = 'maxlength="32"'
ssid5.placeholder = translate("SSID is required")
ssid5.rmempty = false

enc5 = s5:taboption("wireless5", ListValue, "encryption", translate("Encryption"))
enc5:value("none", translate("No Encryption"))
enc5:value("mixed-psk", translate("mixed-psk"))
function enc5.write(self, section, value)
	self.map.uci:set("wireless", section, "encryption", value)
end

key5 = s5:taboption("wireless5", Value, "key", translate("Key"))
key5.datatype = "wpakey"
key5.placeholder = translate("Key")
key5.password = true
key5:depends("encryption","mixed-psk")
key5.errmsg = translate("Key can't be empty")
function key5.write(self, section, value)
	self.map.uci:set("wireless", section, "key", value)
end
hidden5 = s5:taboption("wireless5", Flag, "hidden", translate("Hide SSID"))

sr5 = m2:section(NamedSection, "wifi1", translate("Wireless 2.4G Settings"))
sr5.anonymous = true

sr5:tab("wireless5_radio", translate("Advanced"))

hw5 = sr5:taboption("wireless5_radio", ListValue, "hwmode", translate("HW Mode"))

if wifi1_mode == "11ng" or wifi1_mode == "11axg" then
	if wifi1_mode == "11axg" then
		hw5:value("11axg", "11axg")
	end

	hw5:value("11ng", "11ng")
	hw5:value("11g", "11g")
	hw5:value("11b", "11b")
else
	if wifi1_mode == "11axa" then
		hw5:value("11axa", "11axa")
	end

	hw5:value("11ac", "11ac")
	hw5:value("11na", "11na")
	hw5:value("11a", "11a")
end

local iw5 = luci.sys.wifi.getiwinfo("wifi1")

ch5 = sr5:taboption("wireless5_radio", ListValue, "channel", translate("Channel"))
ch5:value("auto", translate("auto"))
for _, freq in ipairs(iw5.freqlist) do
	ch5:value(freq.channel, freq.channel .. " (%d MHz)" % freq.mhz)
end

ht5 = sr5:taboption("wireless5_radio", ListValue, "htmode", translate("HT Mode"))
ht5:value("auto", "auto")
if wifi1_mode == "11axa" or wifi1_mode == "11ac" then
	ht5:value("HT80", "HT80")
end
ht5:value("HT40", "HT40")
ht5:value("HT20", "HT20")

local cl5 = iw5 and iw5.countrylist
if cl5 and #cl5 > 0 then
	cc5 = sr5:taboption("wireless5_radio", ListValue, "country", translate("Country Code"), translate("Use ISO/IEC 3166 alpha2 country codes."))
	cc5.default = tostring(iw5 and iw5.country or "00")
	for _, c in ipairs(cl5) do
		cc5:value(c.alpha2, "%s - %s" %{ c.alpha2, c.name })
	end
else
	sr5:taboption("wireless5_radio", Value, "country", translate("Country Code"), translate("Use ISO/IEC 3166 alpha2 country codes."))
end

function m2.on_commit(map)
	if map.changed == true then
		fork_exec("wifi up wifi1")
	end
end

return m,m2
