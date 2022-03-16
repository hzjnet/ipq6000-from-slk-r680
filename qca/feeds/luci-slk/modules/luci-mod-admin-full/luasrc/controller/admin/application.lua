-- Copyright 2008 Steven Barth <steven@midlink.org>
-- Licensed to the public under the Apache License 2.0.

module("luci.controller.admin.application", package.seeall)

function index()
	entry({"admin", "application"}, alias("admin","application"), _("Application Manage"), 60)	
	--entry({"admin", "application" ,"frp"}, cbi("admin_application/frp"), _("FRP"), 1)

end