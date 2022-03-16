-- Copyright 2008 Steven Barth <steven@midlink.org>
-- Licensed to the public under the Apache License 2.0.

module("luci.controller.admin.vpn", package.seeall)

function index()
	entry({"admin", "vpn"}, alias("admin","vpn","pptp"), _("VPN Service"), 70)	
	entry({"admin", "vpn" ,"pptp"}, cbi("admin_vpn/pptp"), _("PPTP VPN"), 1)
	entry({"admin", "vpn" ,"l2tp"}, cbi("admin_vpn/l2tp"), _("L2TP VPN"), 2)
	--entry({"admin", "vpn" ,"ipsec"}, cbi("admin_vpn/ipsec"), _("IPSec VPN"), 3)	
	--entry({"admin", "vpn" ,"gpe"}, cbi("admin_vpn/gre"), _("GRE VPN"), 4)
	if (string.gsub(luci.sys.exec("uci get system.@system[0].model"),"%s+","") =="WL-245N-S") then
		entry({"admin", "vpn" ,"openvpn"}, cbi("admin_vpn/openvpn"), _("WiVPN"), 5)
	else
		entry({"admin", "vpn" ,"openvpn"}, cbi("admin_vpn/openvpn"), _("OpenVPN"), 5)
	end
	--entry({"admin", "vpn" ,"openvpn"}, cbi("admin_vpn/openvpn"), _("WiVPN"), 5)
end