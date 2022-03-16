module("luci.controller.modem", package.seeall)

function index()
	if nixio.fs.access("/etc/config/modem") then
		if (string.gsub(luci.sys.exec('uci -q get modem.@ndis[0].model |grep nr5g |wc -l'),"%s+","") == "1") then
			page=entry({"admin", "network", "modem"}, cbi("admin_network/modem"), _("5G Modem"), 1) 
		elseif (string.gsub(luci.sys.exec('uci -q get modem.@ndis[0].model |grep lte |wc -l'),"%s+","") == "1") then
			page=entry({"admin", "network", "modem"}, cbi("admin_network/modem"), _("4G Modem"), 1) 
		end
	end
end
