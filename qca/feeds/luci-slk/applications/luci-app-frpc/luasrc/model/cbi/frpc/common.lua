-- Copyright 2019 Xingwang Liao <kuoruan@gmail.com>
-- Licensed to the public under the MIT License.

local uci = require "luci.model.uci".cursor()
local util = require "luci.util"
local fs = require "nixio.fs"
local sys = require "luci.sys"

local m, s, o
local server_table = { }

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


uci:foreach("frpc", "server", function(s)
	if s.alias then
		server_table[s[".name"]] = s.alias
	elseif s.server_addr and s.server_port then
		local ip = s.server_addr
		if s.server_addr:find(":") then
			ip = "[%s]" % s.server_addr
		end
		server_table[s[".name"]] = "%s:%s" % { ip, s.server_port }
	end
end)

local function frpc_version()
	local file = uci:get("frpc", "main", "client_file")

	if not file or file == "" or not fs.stat(file) then
		return "<em style=\"color: red;\">%s</em>" % translate("Invalid client file")
	end

	if not fs.access(file, "rwx", "rx", "rx") then
		fs.chmod(file, 755)
	end

	local version = util.trim(sys.exec("%s -v 2>/dev/null" % file))
	if version == "" then
		return "<em style=\"color: red;\">%s</em>" % translate("Can't get client version")
	end
	return translatef("Version: %s", version)
end

m = Map("frpc", "%s - %s" % { translate("Frpc"), translate("Common Settings") },
"<p>%s</p>" % {
	translate("Frp is a fast reverse proxy to help you expose a local server behind a NAT or firewall to the internet.")})

m:append(Template("frpc/status_header"))

s = m:section(NamedSection, "main", "frpc")
s.addremove = false
s.anonymous = true

s:tab("general", translate("General Options"))
s:tab("advanced", translate("Advanced Options"))

--en = s:option(Button, "__toggle")

--local disableds=string.gsub(luci.sys.exec("uci get wireless.@wifi-iface[1].disabled"),"\n","")
--[[if wdev:get("disabled") == "1" or wnet:get("disabled") == "1" then
	en.title      = translate("Wireless network is disabled ")
	en.inputtitle = translate("Enable")
	en.inputstyle = "apply"
else
	en.title      = translate("Wireless network is enabled ")
	en.inputtitle = translate("Disable")
	en.inputstyle = "reset"
end--]]
--[[
en = s:taboption("general", Button, "__toggle")
local enable = string.gsub(luci.sys.exec("uci get frpc.main.enabled"),"\n","")
if enable == "0" then
	en.title      = translate("Frpc is disabled ")
	en.inputtitle = translate("Enable")
	en.inputstyle = "apply"
else
	en.title      = translate("Frpc is enabled")
	en.inputtitle = translate("Disable")
	en.inputstyle = "reset"
end
function en.parse()
	if m:formvalue("cbid.frpc.main.__toggle") then
		if m:formvalue("cbid.frpc.main.__toggle") == "Enable" then
			luci.sys.exec("uci set frpc.main.enabled=1")
			fork_exec("/etc/slkscript/frpc_down")
		else
			luci.sys.exec("uci set frpc.main.enabled=0")
		end
		
		luci.http.redirect(luci.dispatcher.build_url("admin/application/frpc"))
		return
	end
end
--]]

o = s:taboption("general", Flag, "enabled", translate("Enabled"))

--o = s:taboption("general", Value, "client_file", translate("Client file"),frpc_version())
--o.datatype = "file"
--o.rmempty = false

o = s:taboption("general", ListValue, "server", translate("Server"))
o:value("", translate("None"))
for k, v in pairs(server_table) do
	o:value(k, v)
end

o = s:taboption("general", ListValue, "run_user", translate("Run daemon as user"))
o:value("", translate("-- default --"))
local user
for user in util.execi("cat /etc/passwd | cut -d':' -f1") do
	o:value(user)
end

o = s:taboption("general", Flag, "enable_logging", translate("Enable logging"))

o = s:taboption("general", Value, "log_file", translate("Log file"))
o:depends("enable_logging", "1")
o.placeholder = "/var/log/frpc.log"

o = s:taboption("general", ListValue, "log_level", translate("Log level"))
o:depends("enable_logging", "1")
o:value("trace", translate("Trace"))
o:value("debug", translate("Debug"))
o:value("info", translate("Info"))
o:value("warn", translate("Warn"))
o:value("error", translate("Error"))
o.default = "warn"

o = s:taboption("general", Value, "log_max_days", translate("Log max days"))
o:depends("enable_logging", "1")
o.datatype = "uinteger"
o.placeholder = '3'

o = s:taboption("general", Value, "disable_log_color", translate("Disable log color"))
o:depends("enable_logging", "1")
o.enabled = "true"
o.disabled = "false"

o = s:taboption("advanced", Flag, "login_fail_exit", translate("Login fail exit"))
o.enabled = "true"
o.disabled = "false"
o.defalut = o.enabled
o.rmempty = false

o = s:taboption("advanced", ListValue, "protocol", translate("Protocol"),
	translate("Communication protocol used to connect to server, default is tcp"))
o:value("tcp", "TCP")
o:value("kcp", "KCP")
o:value("websocket", "Websocket")
o.default = "tcp"

o = s:taboption("advanced", Flag, "tls_enable", translate("TLS enable"),
	translate("If true, Frpc will connect Frps by TLS"))
o.enabled = "true"
o.disabled = "false"

o = s:taboption("advanced", Value, "dns_server", translate("DNS server"))
o.datatype = "host"

return m
