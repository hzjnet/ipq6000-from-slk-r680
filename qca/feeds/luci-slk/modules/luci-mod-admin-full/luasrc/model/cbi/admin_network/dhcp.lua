-- Copyright 2012 Christian Gagneraud <chris@techworks.ie>
-- Licensed to the public under the Apache License 2.0.

--local nw = require "luci.model.network"
--local net = nw:get_network("wlan0")
local ipc = require "luci.ip"
m = Map("dhcp",
	translate("DHCP Server Settings"), 
	translate("Configurable DHCP Server."))

s = m:section(TypedSection, "dhcp")
s.anonymous = true
--s.addremove = true

--s.addremove = false


local has_section = false

function s.filter(self, section)
	return m.uci:get("dhcp", section, "interface") == "lan"
end

local ignore = s:option(Flag, "ignore",
			translate("Ignore interface"),
			translate("Disable <abbr title=\"Dynamic Host Configuration Protocol\">DHCP</abbr> for " ..
				"this interface."))

local start = s:option( Value, "start", translate("Start"),
		translate("Lowest leased address as offset from the network address."))
--start.optional = true
start.datatype = "or(uinteger,ip4addr)"
start.default = "100"
start:depends("ignore","")


local limit = s:option(Value, "limit", translate("Limit"),
		translate("Maximum number of leased addresses."))
--limit.optional = true
limit.datatype = "uinteger"
limit.default = "150"
limit:depends("ignore","")

local ltime = s:option( Value, "leasetime", translate("Leasetime"),
		translate("Expiry time of leased addresses, minimum is 2 minutes (<code>2m</code>)."))
--ltime.rmempty = true
--ltime.optional = true
ltime.default = "12h"
ltime:depends("ignore","")

s = m:section(TypedSection, "host", translate("Static Leases"),
	translate("Static leases are used to assign fixed IP addresses and symbolic hostnames to " ..
		"DHCP clients. They are also required for non-dynamic interface configurations where " ..
		"only hosts with a corresponding lease are served.") .. "<br />" ..
	translate("Use the <em>Add</em> Button to add a new lease entry. The <em>MAC-Address</em> " ..
		"indentifies the host, the <em>IPv4-Address</em> specifies to the fixed address to " ..
		"use and the <em>Hostname</em> is assigned as symbolic name to the requesting host."))

s.addremove = true
s.anonymous = true
s.template = "cbi/tblsection"

name = s:option(Value, "name", translate("Hostname"))
name.datatype = "hostname"
name.rmempty  = true

mac = s:option(Value, "mac", translate("<abbr title=\"Media Access Control\">MAC</abbr>-Address"))
mac.datatype = "list(macaddr)"
mac.rmempty  = true

ip = s:option(Value, "ip", translate("<abbr title=\"Internet Protocol Version 4\">IPv4</abbr>-Address"))
ip.datatype = "or(ip4addr,'ignore')"

hostid = s:option(Value, "hostid", translate("<abbr title=\"Internet Protocol Version 6\">IPv6</abbr>-Suffix (hex)"))

ipc.neighbors({ family = 4 }, function(n)
	if n.mac and n.dest then
		ip:value(n.dest:string())
		mac:value(n.mac, "%s (%s)" %{ n.mac, n.dest:string() })
	end
end)

function ip.validate(self, value, section)
	local m = mac:formvalue(section) or ""
	local n = name:formvalue(section) or ""
	if value and #n == 0 and #m == 0 then
		return nil, translate("One of hostname or mac address must be specified!")
	end
	return Value.validate(self, value, section)
end

-----------命令--------------
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
	luci.sys.exec("/etc/init.d/dnsmasq restart")
end

return m
