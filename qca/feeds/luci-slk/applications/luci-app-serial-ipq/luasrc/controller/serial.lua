module("luci.controller.serial", package.seeall)

function index()
	if not nixio.fs.access("/dev/ttyMSM1") then
		return
	end
	entry({"admin", "application","serial"}, cbi("serial/serial"), _("Serial Utility"), 1)	
end
