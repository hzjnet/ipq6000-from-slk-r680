-- Copyright 2008 Steven Barth <steven@midlink.org>
-- Copyright 2011 Jo-Philipp Wich <jow@openwrt.org>
-- Licensed to the public under the Apache License 2.0.

local sys   = require "luci.sys"
local zones = require "luci.sys.zoneinfo"
local fs    = require "nixio.fs"
local conf  = require "luci.config"

local m, s, o

m = Map("system", translate("Language Setting"))
m:chain("luci")


s = m:section(TypedSection, "system", translate("Language Setting"))
s.anonymous = true
s.addremove = false


--
-- Langauge & Style
--

o = s:option(ListValue, "_lang", translate("Language"))
o:value("auto")

local i18ndir = luci.i18n.i18ndir .. "base."
for k, v in luci.util.kspairs(conf.languages) do
	local file = i18ndir .. k:gsub("_", "-")
	if k:sub(1, 1) ~= "." and fs.access(file .. ".lmo") then
		o:value(k, v)
	end
end

function o.cfgvalue(...)
	return m.uci:get("luci", "main", "lang")
end

function o.write(self, section, value)
	m.uci:set("luci", "main", "lang", value)
end



function m.on_commit(map)
	local comis = luci.sys.exec("uci commit system")
	if comis then
		luci.http.redirect(luci.dispatcher.build_url("admin/system/language"))
	end

end


return m
