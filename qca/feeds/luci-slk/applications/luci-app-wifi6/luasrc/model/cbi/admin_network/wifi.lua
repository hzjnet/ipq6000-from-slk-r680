-- Copyright 2012 Christian Gagneraud <chris@techworks.ie>
-- Licensed to the public under the Apache License 2.0.

local nw = require "luci.model.network"


m = Map("wificonfig",translate("Wireless Network"),translate("Configure WiFi 2.4G"))

nw.init(m.uci)

s = m:section(NamedSection,"wlan1","wifi-iface")
s.anonymous = true
s.addremove = false

s:tab("basic", translate("Basic Configuration"))
s:tab("advanced", translate("Advanced Configuration"))

en=s:taboption("basic", Flag, "en", translate("Enabled"))

ssid=s:taboption("basic", Value, "ssid", translate("WiFi Name"))
ssid.default="WiFi_2G"

security = s:taboption("basic", ListValue, "security", translate("Security"))
security:value("none", translate("None"))
security:value("encryption", translate("Encryption"))

key=s:taboption("basic", Value, "key", translate("WiFi Key"))
key.password=true
key:depends({security="encryption"})
key.default="slk100200"

hidden=s:taboption("advanced", Flag, "hidden", translate("Hide ESSID"))


------------------------channel-------------------------
channel=s:taboption("advanced", ListValue, "channel", translate("Channel"))
channel.default="auto"
channel:value("auto",translate("auto"))
channel:value("1","1 (2412MHz)")
channel:value("2","2 (2417MHz)")
channel:value("3","3 (2422MHz)")
channel:value("4","4 (2427MHz)")
channel:value("5","5 (2432MHz)")
channel:value("6","6 (2437MHz)")
channel:value("7","7 (2442MHz)")
channel:value("8","8 (2447MHz)")
channel:value("9","9 (2452MHz)")
channel:value("10","10 (2457MHz)")
channel:value("11","11 (2462MHz)")
--channel:value("12","12 (2472MHz)")
--channel:value("13","13 (2477MHz)")
--channel:value("14","14 (2484MHz)")

htmode=s:taboption("advanced", ListValue, "htmode", translate("Width"))
htmode.default=htmoded
htmode:value("HT20","HT20")
htmode:value("HT40","HT40")

txpower=s:taboption("advanced", ListValue, "txpower", translate("Transmit Power"))
txpower:value("15",translate("Low"))
txpower:value("21",translate("Medium"))
txpower:value("27",translate("High"))


m2 = Map("wificonfig"," ",translate("Configure WiFi 5.8G"))

s2 = m2:section(NamedSection,"wlan0","wifi-iface")
s2.anonymous = true
s2.addremove = false

s2:tab("basic", translate("Basic Configuration"))
s2:tab("advanced", translate("Advanced Configuration"))

en=s2:taboption("basic", Flag, "en", translate("Enabled"))

ssid=s2:taboption("basic", Value, "ssid", translate("WiFi Name"))
ssid.default="WiFi_5G"

security = s2:taboption("basic", ListValue, "security", translate("Security"))
security:value("none", translate("None"))
security:value("encryption", translate("Encryption"))

key=s2:taboption("basic", Value, "key", translate("WiFi Key"))
key:depends({security="encryption"})
key.password=true
key.default="slk100200"

hidden=s2:taboption("advanced", Flag, "hidden", translate("Hide ESSID"))

channel=s2:taboption("advanced", ListValue, "channel", translate("Channel"))
channel.default="auto"
channel:value("auto",translate("auto"))
channel:value("36","36 (5180MHz)")
channel:value("40","40 (5200MHz)")
channel:value("44","44 (5220MHz)")
channel:value("48","48 (5240MHz)")
channel:value("52","52 (5260MHz)")
channel:value("56","56 (5280MHz)")
channel:value("60","60 (5300MHz)")
channel:value("64","64 (5320MHz)")
channel:value("149","149 (5745MHz)")
channel:value("153","153 (5765MHz)")
channel:value("157","157 (5785MHz)")
channel:value("161","161 (5805MHz)")
channel:value("165","165 (5825MHz)")
 
channel:value("100","100 (5500MHz)")
channel:value("104","104 (5520MHz)")
channel:value("108","108 (5540MHz)")
channel:value("112","112 (5660MHz)")
channel:value("116","116 (5580MHz)")
channel:value("120","120 (5600MHz)")
channel:value("124","124 (5620MHz)")
channel:value("128","128 (5640MHz)")
channel:value("136","136 (5680MHz)")
channel:value("140","140 (5700MHz)")
channel:value("144","144 (5720MHz)")

htmode=s2:taboption("advanced", ListValue, "htmode", translate("Width"))
htmode.default=htmoded
htmode:value("HT20","HT20")
htmode:value("HT40","HT40")
htmode:value("HT80","HT80")

txpower=s2:taboption("advanced", ListValue, "txpower", translate("Transmit Power"))
txpower:value("15",translate("Low"))
txpower:value("21",translate("Medium"))
txpower:value("27",translate("High"))

local apply = luci.http.formvalue("cbi.apply")
if apply then
    io.popen("/etc/init.d/wifi_init restart")
end

return m,m2
