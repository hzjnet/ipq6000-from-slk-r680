local sys   = require "luci.sys"
local m, s

m = Map("network", translate("Network Configuration"))
m:chain("luci")

s = m:section(NamedSection, "wan", "interface", translate("WAN Configuration"))
s.anonymous = true
s.addremove = false


local function set_status(self)
	st.template = "admin_network/iface_status"
	st.network  = self
	st.value    = nil
end

st = s:option(DummyValue, "__statuswan", translate("Status"))
st.on_init = set_status("wan")

--static
protocol = s:option(ListValue, "proto", translate("Protocol"))
protocol.default = "dhcp"
protocol:value("pppoe", translate("PPPoE"))
protocol:value("dhcp", translate("DHCP address"))
protocol:value("static", translate("Static address"))
protocol:value("aslan", translate("As lan"))
protocol:value("disable", translate("Disable"))

ipaddr = s:option(Value, "ipaddr", translate("IP Address"))
ipaddr.datatype = "ip4addr"
ipaddr.default = "192.168.1.100"
ipaddr:depends({proto="static"})

netmask = s:option(Value, "netmask",translate("Netmask"))
netmask.datatype = "ip4addr"
netmask.default = "255.255.255.0"
netmask:value("255.255.255.0")
netmask:value("255.255.0.0")
netmask:value("255.0.0.0")
netmask:depends({proto="static"})

gateway= s:option(Value, "gateway", translate("Gateway"))
gateway.datatype = "ip4addr"
gateway:depends({proto="static"})

dns= s:option(DynamicList, "dns", translate("DNS"))
dns.datatype = "ip4addr"
dns.cast     = "string"
dns:depends({proto="static"})


-----------------------PPPoE--------------------------

username = s:option(Value, "username",translate("Username"))
username:depends({proto="pppoe"})

password = s:option(Value, "password",translate("Password"))
password:depends({proto="pppoe"})
password.password = true

local apply = luci.http.formvalue("cbi.apply")
if apply then
    io.popen("/etc/init.d/netwan start")
end

return m
