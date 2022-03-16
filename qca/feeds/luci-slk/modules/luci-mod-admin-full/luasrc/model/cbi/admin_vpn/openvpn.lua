-- Copyright 2008 Steven Barth <steven@midlink.org>
-- Licensed to the public under the Apache License 2.0.

require("luci.ip")
require("luci.model.uci")


--local basicParams = {
	--								
	-- Widget, Name, Default(s), Description
	--
					
--	{ ListValue, "verb", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, translate("Set output verbosity") },
--	{ Value, "nice",0, translate("Change process priority") },
--	{ Value,"port",1194, translate("TCP/UDP port # for both local and remote") },
--	{ ListValue,"dev_type",{ "tun", "tap" }, translate("Type of used device") },
--	{ Flag,"tun_ipv6",0, translate("Make tun device IPv6 capable") },

--	{ Value,"ifconfig","10.200.200.3 10.200.200.1", translate("Set tun/tap adapter parameters") },
--	{ Value,"server","10.200.200.0 255.255.255.0", translate("Configure server mode") },
--	{ Value,"server_bridge","192.168.1.1 255.255.255.0 192.168.1.128 192.168.1.254", translate("Configure server bridge") },
--	{ Flag,"nobind",0, translate("Do not bind to local address and port") },

--	{ ListValue,"comp_lzo",{"yes","no","adaptive"}, translate("Use fast LZO compression") },
--	{ Value,"keepalive","10 60", translate("Helper directive to simplify the expression of --ping and --ping-restart in server mode configurations") },

--	{ ListValue,"proto",{ "udp", "tcp" }, translate("Use protocol") },

--	{ Flag,"client",0, translate("Configure client mode") },
--	{ Flag,"client_to_client",0, translate("Allow client-to-client traffic") },
--	{ DynamicList,"remote","vpnserver.example.org", translate("Remote host name or ip address") },

--	{ FileUpload,"secret","/etc/openvpn/secret.key 1", translate("Enable Static Key encryption mode (non-TLS)") },
--	{ FileUpload,"pkcs12","/etc/easy-rsa/keys/some-client.pk12", translate("PKCS#12 file containing keys") },
--	{ FileUpload,"ca","/etc/easy-rsa/keys/ca.crt", translate("Certificate authority") },
--	{ FileUpload,"dh","/etc/easy-rsa/keys/dh1024.pem", translate("Diffie Hellman parameters") },
--	{ FileUpload,"cert","/etc/easy-rsa/keys/some-client.crt", translate("Local certificate") },
--	{ FileUpload,"key","/etc/easy-rsa/keys/some-client.key", translate("Local private key") },
--}
--adaptive
local basicParams = {
	--								
	-- Widget, Name, Default(s), Description
	--
					
	
	--{ ListValue,"dev_type",{ "tun", "tap" }, translate("Type of used device") },
	{ "general",ListValue,"dev",{ "tun", "tap" },	translate("tun/tap device") },
	{ "general",ListValue,"proto",{ "udp", "tcp" }, translate("Use protocol") },
	{ "general",Value,"port",1194, translate("TCP/UDP port # for both local and remote") },

	--{ ListValue, "verb", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, translate("Set output verbosity") },
	--{ Flag,"nobind",0, translate("Do not bind to local address and port") },
	--{ Flag,"client",0, translate("Configure client mode") },
	--{ Flag,"client_to_client",0, translate("Allow client-to-client traffic") },

	{ "general",DynamicList,"remote","vpnserver.example.org", translate("Remote host name or ip address") },

	{ "advanced",Flag,"relink",1, translate("Auto connect server") },
	{ "advanced",ListValue, "verb", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, translate("Set output verbosity") },
	--if string.gsub(luci.sys.exec("uci get system.@system[0].model"),"\n","") ~= "WL-450T-LT" then
	{ "advanced",Value,"auth",	"SHA1",	translate("HMAC authentication for packets") }, -- parse
	--end
	{ "advanced",Value,"cipher","BF-CBC",translate("Encryption cipher for packets") }, -- parse
	{ "advanced",ListValue, "comp__lzo", { "yes","no","adative","disable" }, translate("Set Comp_lzo") },
	{ "advanced",ListValue,"remote_cert_tls",{ "client", "server" },translate("Require explicit key usage on certificate") },


	{ "general",FileUpload,"ca","/etc/easy-rsa/keys/ca.crt", translate("Certificate authority") },
	{ "general",FileUpload,"cert","/etc/easy-rsa/keys/some-client.crt", translate("Local certificate") },
	{ "general",FileUpload,"key","/etc/easy-rsa/keys/some-client.key", translate("Local private key") },


	{ "advanced",Flag,"nobind",0, translate("Do not bind to local address and port") },
	{ "advanced",Flag,"client",0, translate("Configure client mode") },
	{ "advanced",Flag,"client_to_client",0, translate("Allow client-to-client traffic") },

}

local seniorParams = {
	{ ListValue, "verb", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, translate("Set output verbosity") },
	{ Flag,"nobind",0, translate("Do not bind to local address and port") },
	{ Flag,"client",0, translate("Configure client mode") },
	{ Flag,"client_to_client",0, translate("Allow client-to-client traffic") },
}
local function set_status(self)
	-- if current network is empty, print a warning
	--if not net:is_floating() and net:is_empty() then
	--	st.template = "cbi/dvalue"
	--	st.network  = nil
	--	st.value    = translate("There is no device assigned yet, please attach a network device in the \"Physical Settings\" tab")
	--else
		st.template = "admin_network/iface_status"
		st.network  = self
		st.value    = nil
	--end
end


local names="OpenVPN"
if string.gsub(luci.sys.exec("uci get system.@system[0].model"),"%s+","") == "WL-245N-S" then
	names="WiVPN"
end
local m = Map("openvpn", translate(""..names.." Settings"))
--local p = m:section( SimpleSection )

--p.template = "openvpn/pageswitch"
--p.mode     = "basic"


local s = m:section( NamedSection, "sample_client", "openvpn" )

s:tab("general", translate("General Settings"))
s:tab("advanced", translate("Advanced Settings"))
--p.instance = "sample_client"--arg[1]arg[1]
st = s:taboption("general",DummyValue, "__statuswifi", translate("Status"))
st.on_init = set_status("openvpn")
enabled = s:taboption("general",Flag, "enabled", translate("Enable"))



for _, option in ipairs(basicParams) do
	local o = s:taboption(
		option[1],option[2], option[3],
		option[3], option[5]
	)
	
	--o.optional = true

	if option[2] == DummyValue then
		o.value = option[4]
	else
		if option[2] == DynamicList then
			o.cast = nil
			function o.cfgvalue(...)
				local val = AbstractValue.cfgvalue(...)
				return ( val and type(val) ~= "table" ) and { val } or val
			end
		end

		if type(option[4]) == "table" then
			if o.optional then o:value("", "-- remove --") end
			for _, v in ipairs(option[4]) do
				v = tostring(v)
				o:value(v)
			end
			o.default = tostring(option[4][2])
		else
			o.default = tostring(option[4])
		end
	end

	for i=5,#option do
		if type(option[i]) == "table" then
			o:depends(option[i])
			
		end
		if option[1] then
			if string.gsub(luci.sys.exec("uci get system.@system[0].model"),"%s+","") == "WL-450T-LT" then
				if option[3] == "auth" then
					o:depends({enabled="2"})
				else
					o:depends({enabled="1"})
				end
			else
				o:depends({enabled="1"})
			end
			--
		end
	end
end

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



function m.on_commit(map)
	local isifece=string.gsub(luci.sys.exec("uci get openvpn.sample_client.dev"),"\n","")
	luci.sys.exec("uci set network.openvpn.ifname="..isifece.."0")
	local commits=luci.sys.exec("uci commit network")
	if commits then

		if luci.http.formvalue("cbid.openvpn.sample_client.relink") == "1" then
		    
			--fork_exec("/etc/init.d/slkinit stop re_openvpn")
			fork_exec("/etc/init.d/slkinit start re_openvpn")
		else
			fork_exec("/etc/init.d/slkinit stop re_openvpn")
		end
		local comp_lzos=luci.http.formvalue("cbid.openvpn.sample_client.comp__lzo")
			if comp_lzos then
				if comp_lzos == "disable" then
					luci.sys.exec("uci del openvpn.sample_client.comp_lzo")
				else
					luci.sys.exec("uci set openvpn.sample_client.comp_lzo="..comp_lzos.."")
				end
				luci.sys.exec("uci commit openvpn")
			end
		local enableds = luci.http.formvalue("cbid.openvpn.sample_client.enabled")--luci.sys.exec("uci -q get openvpn.sample_client.enabled")
		if enableds == "1" then
			fork_exec("/etc/init.d/openvpn start sample_client &")
		else
			fork_exec("/etc/init.d/openvpn stop &")
		end
	end
end
--[[
local submits=luci.http.formvalue("cbi.submit")
if submit then
		local comp_lzos=luci.http.formvalue("cbid.openvpn.sample_client.comp__lzo")
		if comp_lzos then
			if comp_lzos == "disable" then
				luci.sys.exec("uci del openvpn.sample_client.comp_lzo")
			else
				luci.sys.exec("uci set openvpn.sample_client.comp_lzo="..comp_lzos.."")
			end
			luci.sys.exec("uci commit openvpn")
		end

end
--]]
return m
