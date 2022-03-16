-- Copyright 2008 Steven Barth <steven@midlink.org>
-- Licensed to the public under the Apache License 2.0.

module("luci.controller.admin.route", package.seeall)

function index()
	entry({"admin", "route"}, alias("admin","route","routing"), _("Routing Setting"), 50)	
	--entry({"admin", "route" ,"nat"}, arcombine(cbi("admin_route/rules"), cbi("admin_route/rule-details")), _("NAT"), 1)
	entry({"admin", "route" ,"routing"}, cbi("admin_route/routing"), _("Routing Table"), 1)	
	--entry({"admin", "route" ,"netbackup"}, cbi("admin_route/netbackup"), _("Network Backup"), 5)
	--entry({"admin", "route" ,"diagnosis"}, template("admin_network/diagnostics"), _("Diagnosis"), 2)
	entry({"admin", "route", "diag_ping"}, call("diag_ping"), nil).leaf = true
	entry({"admin", "route" ,"firewall"}, cbi("admin_route/firewallon"), _("Firewall"), 3)
	entry({"admin", "route" ,"mapping"}, arcombine(cbi("admin_route/forwards"), cbi("admin_route/forward-details")), _("Port Mapping"), 4)
	entry({"admin", "route" ,"dmz"}, cbi("admin_route/dmz"), _("DMZ"), 5)
end

