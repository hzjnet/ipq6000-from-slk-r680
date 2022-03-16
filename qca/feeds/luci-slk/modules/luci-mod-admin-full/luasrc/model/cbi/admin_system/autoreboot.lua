--[[
LuCI - Lua Configuration Interface

Copyright 2010 Jo-Philipp Wich <xm@subsignal.org>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0
]]--
local m2,m, section

m = Map("system", translate("Device Reboot"), translate("Configure Device Reboot"))
m:chain("luci")
s2 = m:section(TypedSection, "system")
button = s2:option(ListValue, "_reboot")
button.template = "admin_system/reboots"
s2.anonymous = true
s2.addremove = false

--[[
button = s:option(Button, "__reboot", translate("reboot"))
function button.write()
		luci.template.render("admin_system/reboot", {reboot=reboot})
	if reboot then
		luci.sys.reboot()
	end
end
--]]
m2 = Map("autoreboot", translate("Time Reboot"), translate("Configure a timed reboot"))
s = m2:section(TypedSection, "login", "")
s.addremove = false
s.anonymous = true

o = s:option(DummyValue, "_systime", translate("Local Time"))
o.template = "admin_system/clock_status"

enable = s:option(Flag, "enable", translate("Enable"),translate("Enable the device to restart during the configured time each day (the recommended time is between 23:00 and 6:00 PM)."))

hour = s:option(ListValue, "hour", translate("hour"))
hour:value("0", "00")
hour:value("1", "01")
hour:value("2", "02")
hour:value("3", "03")
hour:value("4", "04")
hour:value("5", "05")
hour:value("6", "06")
hour:value("7", "07")
hour:value("8", "08")
hour:value("9", "09")
hour:value("10", "10")
hour:value("11", "11")
hour:value("12", "12")
hour:value("13", "13")
hour:value("14", "14")
hour:value("15", "15")
hour:value("16", "16")
hour:value("17", "17")
hour:value("18", "18")
hour:value("19", "19")
hour:value("20", "20")
hour:value("21", "21")
hour:value("22", "22")
hour:value("23", "23")

pass = s:option(ListValue, "minute", translate("minute"))
pass:value("0", "00")
pass:value("5", "05")
pass:value("10", "10")
pass:value("15", "15")
pass:value("20", "20")
pass:value("25", "25")
pass:value("30", "30")
pass:value("35", "35")
pass:value("40", "40")
pass:value("45", "45")
pass:value("50", "50")
pass:value("55", "55")





local apply = luci.http.formvalue("cbi.apply")
if apply then
	io.popen("/etc/init.d/autoreboot restart")
end

return m,m2
