--[[
LuCI - Lua Configuration Interface

LuCI is a personal plaything which leads our developer
to be conflused and anxious.

]]--

local m, s, o, n
--translate("Firewall") .. " - " .. "DMZ",
m = Map("filter",
	translate("DMZ"),
	translate("The DMZ host feature allows one local host to be exposed \
		to the Internet for a special-purpose service."))

s = m:section(TypedSection, "dmz", translate("Configuration"))
s.addremove = false
s.anonymous = true

o = s:option(Flag, "enabled", translate("Enable"))

o = s:option(Value, "dest_ip", translate("Internal IP address"))
o.datatype = "ip4addr"
luci.sys.net.ipv4_hints(function(ip, name)
        o:value(ip, "%s (%s)" %{ ip, name })
end)
function m.on_commit(map)
	local comis = luci.sys.exec("uci commit filter")
	if comis then
		luci.sys.exec("/etc/init.d/filter restart")
		--luci.sys.exec("/etc/init.d/firewall restart")
		--luci.http.redirect(luci.dispatcher.build_url("admin/route/firewall"))
	end
end
return m