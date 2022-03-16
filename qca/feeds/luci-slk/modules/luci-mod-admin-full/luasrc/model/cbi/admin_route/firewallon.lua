-- Copyright 2008 Steven Barth <steven@midlink.org>
-- Licensed to the public under the Apache License 2.0.

local sys   = require "luci.sys"
local zones = require "luci.sys.zoneinfo"
local fs    = require "nixio.fs"
local conf  = require "luci.config"

local m, s, o

m = Map("firewall", translate("Firewall"))
m:chain("luci")


s = m:section(TypedSection, "defaults")
s.anonymous = true
s.addremove = false


o = s:option(ListValue, "forward", translate("Firewall"))
o:value("REJECT",translate("Enable"))
o:value("ACCEPT",translate("Disable"))
function o.write(self, section, value)
	luci.sys.exec("uci set firewall.@defaults[0].forward="..value.."")
	--luci.sys.exec("uci set firewall.@zone[0].forward="..value.."")
	luci.sys.exec("uci set firewall.@zone[1].forward="..value.."")
	luci.sys.exec("uci set firewall.@zone[1].input="..value.."")
	luci.sys.exec("uci commit firewall")
end

function m.on_commit(map)
	local comis = luci.sys.exec("uci commit firewall")
	if comis then
		luci.sys.exec("/etc/init.d/firewall restart")
		--luci.http.redirect(luci.dispatcher.build_url("admin/route/firewall"))
	end
end

return m