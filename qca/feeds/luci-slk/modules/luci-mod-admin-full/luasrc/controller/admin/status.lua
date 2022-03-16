-- Copyright 2008 Steven Barth <steven@midlink.org>
-- Copyright 2011 Jo-Philipp Wich <jow@openwrt.org>
-- Licensed to the public under the Apache License 2.0.

module("luci.controller.admin.status", package.seeall)

function index()
	entry({"admin", "status"}, alias("admin", "status", "overview"), _("Routing Status"), 10).index = true
	entry({"admin", "status", "overview"}, template("admin_status/index"), _("Status"), 1)
	entry({"admin", "status", "headers"}, call("action_header"), nil).leaf = true
	entry({"admin", "status", "routes"}, template("admin_status/routes"), _("Routes"), 3)
	entry({"admin", "status", "syslog"}, call("action_syslog"), _("System Log"), 4)

end

function action_header()

	local modem_cpsi="NO"
	local modem_signal="NO"
	if nixio.fs.access("/tmp/SIM") then
		modem_cpsi=string.gsub(luci.sys.exec("cat /tmp/SIM | grep '+CPSI:' |awk -F ' ' '{print $2}'"),"%s+","")
		if modem_cpsi == nil or modem_cpsi == "" then
			modem_cpsi = "NO"
		end
	
		modem_signal=string.gsub(luci.sys.exec("cat /tmp/SIM | grep '+SVAL:' |awk -F ' ' '{print $2}'"),"%s+","")
		if modem_signal == nil or modem_signal == "" then
			modem_signal = "NO"
		end
	end
	
	local rv={
		status="OK",
		modem_cpsi=modem_cpsi,
		modem_signal=modem_signal,
	};
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
	return
end
function action_syslog()
	local syslog = luci.sys.syslog()
	luci.template.render("admin_status/syslog", {syslog=syslog})
end

function action_dmesg()
	local dmesg = luci.sys.dmesg()
	luci.template.render("admin_status/dmesg", {dmesg=dmesg})
end

function action_iptables()
	if luci.http.formvalue("zero") then
		if luci.http.formvalue("zero") == "6" then
			luci.util.exec("ip6tables -Z")
		else
			luci.util.exec("iptables -Z")
		end
		luci.http.redirect(
			luci.dispatcher.build_url("admin", "status", "iptables")
		)
	elseif luci.http.formvalue("restart") == "1" then
		luci.util.exec("/etc/init.d/firewall reload")
		luci.http.redirect(
			luci.dispatcher.build_url("admin", "status", "iptables")
		)
	else
		luci.template.render("admin_status/iptables")
	end
end

function action_bandwidth(iface)
	luci.http.prepare_content("application/json")

	local bwc = io.popen("luci-bwc -i %q 2>/dev/null" % iface)
	if bwc then
		luci.http.write("[")

		while true do
			local ln = bwc:read("*l")
			if not ln then break end
			luci.http.write(ln)
		end

		luci.http.write("]")
		bwc:close()
	end
end

function action_wireless(iface)
	luci.http.prepare_content("application/json")

	local bwc = io.popen("luci-bwc -r %q 2>/dev/null" % iface)
	if bwc then
		luci.http.write("[")

		while true do
			local ln = bwc:read("*l")
			if not ln then break end
			luci.http.write(ln)
		end

		luci.http.write("]")
		bwc:close()
	end
end

function action_load()
	luci.http.prepare_content("application/json")

	local bwc = io.popen("luci-bwc -l 2>/dev/null")
	if bwc then
		luci.http.write("[")

		while true do
			local ln = bwc:read("*l")
			if not ln then break end
			luci.http.write(ln)
		end

		luci.http.write("]")
		bwc:close()
	end
end

function action_connections()
	local sys = require "luci.sys"

	luci.http.prepare_content("application/json")

	luci.http.write("{ connections: ")
	luci.http.write_json(sys.net.conntrack())

	local bwc = io.popen("luci-bwc -c 2>/dev/null")
	if bwc then
		luci.http.write(", statistics: [")

		while true do
			local ln = bwc:read("*l")
			if not ln then break end
			luci.http.write(ln)
		end

		luci.http.write("]")
		bwc:close()
	end

	luci.http.write(" }")
end

function action_nameinfo(...)
	local i
	local rv = { }
	for i = 1, select('#', ...) do
		local addr = select(i, ...)
		local fqdn = nixio.getnameinfo(addr)
		rv[addr] = fqdn or (addr:match(":") and "[%s]" % addr or addr)
	end

	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end
