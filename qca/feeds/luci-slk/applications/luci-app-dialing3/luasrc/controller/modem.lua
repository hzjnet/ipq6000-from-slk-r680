module("luci.controller.modem", package.seeall)

function index()
	if nixio.fs.access("/etc/config/modem") then
		local get_sim1=luci.sys.exec("uci -q get modem.SIM")
		if get_sim ~= "" or get_sim ~= nil then
			page=entry({"admin", "network", "modem"}, cbi("admin_network/modem"), _("SIM"), 1)
		end
	end
end
