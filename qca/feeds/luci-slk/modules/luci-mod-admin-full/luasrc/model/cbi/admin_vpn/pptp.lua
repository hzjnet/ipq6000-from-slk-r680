-- Copyright 2008 Steven Barth <steven@midlink.org>
-- Licensed to the public under the Apache License 2.0.

require("luci.ip")
require("luci.model.uci")

local function set_status(self)
	st.template = "admin_network/iface_status"
	st.network  = self
	st.value    = nil
end

local m = Map("network",
				translate("PPTP Client"), 
				translate("Configurable PPTP access to VPN."))

local s = m:section( NamedSection, "pptp", "interface" )

st = s:option(DummyValue, "__status", translate("Status"))
st.on_init = set_status("pptp")

enable = s:option(Flag, "auto", translate("Enable"))
enable.rmempty  = false

severs=s:option(Value,"server",translate("Server Address"))
username=s:option(Value,"username",translate("Username"))
password=s:option(Value,"password",translate("Password"))
password.password=true
metric=s:option(Value,"metric",translate("Metric"),translate("Configure the priority of this network"))
metric.default=8
return m
