-- Copyright 2008 Steven Barth <steven@midlink.org>
-- Copyright 2008-2011 Jo-Philipp Wich <jow@openwrt.org>
-- Licensed to the public under the Apache License 2.0.

module("luci.controller.admin.system", package.seeall)

function index()
	local fs = require "nixio.fs"

	entry({"admin", "system"}, alias("admin", "system", "system"), _("Equipment Manage"), 80).index = true
	entry({"admin", "system" ,"diagnosis"}, template("admin_network/diagnostics"), _("Diagnosis"), 1)
	entry({"admin", "system", "diag_ping"}, call("diag_ping"), nil).leaf = true
	entry({"admin", "system", "diag_traceroute"}, call("diag_traceroute"), nil).leaf = true
	entry({"admin", "system", "ntp"}, cbi("admin_system/ntp"), _("Date Time"), 2)
	entry({"admin", "system", "clock_status"}, call("action_clock_status"))
	entry({"admin", "system", "language"}, cbi("admin_system/language"),_("Language Setting"), 3)
	entry({"admin", "system", "admin"}, cbi("admin_system/admin"), _("Modify Password"), 4)
	entry({"admin", "system", "flashops"}, call("action_flashops"), _("Update Firmware"), 5)
	entry({"admin", "system", "flashops", "backupfiles"}, form("admin_system/backupfiles"))
	entry({"admin", "system", "factory"}, call("action_factory"), _("Factory Reset"), 6)
	entry({"admin", "system", "ssh"}, cbi("admin_system/ssh"),nil).leaf=true
	entry({"admin", "system", "reboot"}, call("action_reboot"),nil).leaf=true
	entry({"admin", "system", "autoreboot"}, cbi("admin_system/autoreboot"), _("Reboot"), 8)
	entry({"admin", "system" ,"debug"}, template("admin_network/diagdom"), nil)
	entry({"admin", "system", "diag_dom"}, call("diag_dom"), nil).leaf = true
	entry({"admin", "system", "diag_at"}, call("diag_at"), nil).leaf = true
end

function action_clock_status()
	local set = tonumber(luci.http.formvalue("set"))
	if set ~= nil and set > 0 then
		local date = os.date("*t", set)
		if date then
			luci.sys.call("date -s '%04d-%02d-%02d %02d:%02d:%02d'" %{
				date.year, date.month, date.day, date.hour, date.min, date.sec
			})
		end
	end

	luci.http.prepare_content("application/json")
	luci.http.write_json({ timestring = os.date("*t", luci.sys.exec("echo $(date +%s)")) })
end

function action_packages()
	local fs = require "nixio.fs"
	local ipkg = require "luci.model.ipkg"
	local submit = luci.http.formvalue("submit")
	local changes = false
	local install = { }
	local remove  = { }
	local stdout  = { "" }
	local stderr  = { "" }
	local out, err

	-- Display
	local display = luci.http.formvalue("display") or "installed"

	-- Letter
	local letter = string.byte(luci.http.formvalue("letter") or "A", 1)
	letter = (letter == 35 or (letter >= 65 and letter <= 90)) and letter or 65

	-- Search query
	local query = luci.http.formvalue("query")
	query = (query ~= '') and query or nil


	-- Packets to be installed
	local ninst = submit and luci.http.formvalue("install")
	local uinst = nil

	-- Install from URL
	local url = luci.http.formvalue("url")
	if url and url ~= '' and submit then
		uinst = url
	end

	-- Do install
	if ninst then
		install[ninst], out, err = ipkg.install(ninst)
		stdout[#stdout+1] = out
		stderr[#stderr+1] = err
		changes = true
	end

	if uinst then
		local pkg
		for pkg in luci.util.imatch(uinst) do
			install[uinst], out, err = ipkg.install(pkg)
			stdout[#stdout+1] = out
			stderr[#stderr+1] = err
			changes = true
		end
	end

	-- Remove packets
	local rem = submit and luci.http.formvalue("remove")
	if rem then
		remove[rem], out, err = ipkg.remove(rem)
		stdout[#stdout+1] = out
		stderr[#stderr+1] = err
		changes = true
	end


	-- Update all packets
	local update = luci.http.formvalue("update")
	if update then
		update, out, err = ipkg.update()
		stdout[#stdout+1] = out
		stderr[#stderr+1] = err
	end


	-- Upgrade all packets
	local upgrade = luci.http.formvalue("upgrade")
	if upgrade then
		upgrade, out, err = ipkg.upgrade()
		stdout[#stdout+1] = out
		stderr[#stderr+1] = err
	end


	-- List state
	local no_lists = true
	local old_lists = false
	if fs.access("/var/opkg-lists/") then
		local list
		for list in fs.dir("/var/opkg-lists/") do
			no_lists = false
			if (fs.stat("/var/opkg-lists/"..list, "mtime") or 0) < (os.time() - (24 * 60 * 60)) then
				old_lists = true
				break
			end
		end
	end


	luci.template.render("admin_system/packages", {
		display   = display,
		letter    = letter,
		query     = query,
		install   = install,
		remove    = remove,
		update    = update,
		upgrade   = upgrade,
		no_lists  = no_lists,
		old_lists = old_lists,
		stdout    = table.concat(stdout, ""),
		stderr    = table.concat(stderr, "")
	})

	-- Remove index cache
	if changes then
		fs.unlink("/tmp/luci-indexcache")
	end
end

function action_factory()
	local sys = require "luci.sys"
	local fs  = require "nixio.fs"

	local upgrade_avail = fs.access("/lib/upgrade/platform.sh")
	--local reset_avail   = os.execute([[grep -E '"rootfs_data"|"ubi"' /proc/mtd >/dev/null 2>&1]]) == 0

	local restore_cmd = "tar -xzC/ >/dev/null 2>&1"
	local backup_cmd  = "sysupgrade --create-backup - 2>/dev/null"
	local image_tmp   = "/tmp/firmware.img"

	local function image_supported()
		return (os.execute("sysupgrade -T %q >/dev/null" % image_tmp) == 0)
	end

	local function image_checksum()
		return (luci.sys.exec("md5sum %q" % image_tmp):match("^([^%s]+)"))
	end

	local function storage_size()
		local size = 0
		if fs.access("/proc/mtd") then
			for l in io.lines("/proc/mtd") do
				local d, s, e, n = l:match('^([^%s]+)%s+([^%s]+)%s+([^%s]+)%s+"([^%s]+)"')
				if n == "linux" or n == "firmware" then
					size = tonumber(s, 16)
					break
				end
			end
		elseif fs.access("/proc/partitions") then
			for l in io.lines("/proc/partitions") do
				local x, y, b, n = l:match('^%s*(%d+)%s+(%d+)%s+([^%s]+)%s+([^%s]+)')
				if b and n and not n:match('[0-9]') then
					size = tonumber(b) * 1024
					break
				end
			end
		end
		return size
	end


	local fp
	luci.http.setfilehandler(
		function(meta, chunk, eof)
			if not fp then
				if meta and meta.name == "image" then
					fp = io.open(image_tmp, "w")
				else
					fp = io.popen(restore_cmd, "w")
				end
			end
			if chunk then
				fp:write(chunk)
			end
			if eof then
				fp:close()
			end
		end
	)

	if luci.http.formvalue("backup") then
		--
		-- Assemble file list, generate backup
		--
		local reader = ltn12_popen(backup_cmd)
		luci.http.header('Content-Disposition', 'attachment; filename="backup-%s-%s.tar.gz"' % {
			luci.sys.hostname(), os.date("%Y-%m-%d")})
		luci.http.prepare_content("application/x-targz")
		luci.ltn12.pump.all(reader, luci.http.write)
	elseif luci.http.formvalue("restore") then
		--
		-- Unpack received .tar.gz
		--
		local upload = luci.http.formvalue("archive")
		if upload and #upload > 0 then
			luci.template.render("admin_system/applyreboot")
			luci.sys.reboot()
		end
	elseif luci.http.formvalue("image") or luci.http.formvalue("step") then
		--
		-- Initiate firmware flash
		--
		local step = tonumber(luci.http.formvalue("step") or 1)
		if step == 1 then
			if image_supported() then
				luci.template.render("admin_system/upgrade", {
					checksum = image_checksum(),
					storage  = storage_size(),
					size     = (fs.stat(image_tmp, "size") or 0),
					keep     = (not not luci.http.formvalue("keep"))
				})
			else
				fs.unlink(image_tmp)
				luci.template.render("admin_system/factory", {
					--reset_avail   = reset_avail,
					upgrade_avail = upgrade_avail,
					image_invalid = true
				})
			end
		--
		-- Start sysupgrade flash
		--
		elseif step == 2 then
			local keep = (luci.http.formvalue("keep") == "1") and "" or "-n"
			luci.template.render("admin_system/applyreboot", {
				title = luci.i18n.translate("Flashing..."),
				msg   = luci.i18n.translate("The system is flashing now.<br /> DO NOT POWER OFF THE DEVICE!<br /> Wait a few minutes before you try to reconnect. It might be necessary to renew the address of your computer to reach the device again, depending on your settings."),
				addr  = (#keep > 0) and "192.168.2.1" or nil
			})
			fork_exec("sleep 1; killall dropbear uhttpd; sleep 1; /sbin/sysupgrade %s %q" %{ keep, image_tmp })
		end
	elseif luci.http.formvalue("reset") then
		--
		-- Reset system
		--
		luci.template.render("admin_system/applyreboot", {
			title = luci.i18n.translate("Erasing..."),
			msg   = luci.i18n.translate("The system is erasing the configuration partition now and will reboot itself when finished."),
			addr  = "192.168.2.1"
		})
		fork_exec("sleep 1; killall dropbear uhttpd; sleep 1; jffs2reset -y && reboot")
	else
		--
		-- Overview
		--
		luci.template.render("admin_system/factory", {
			reset_avail   = reset_avail,
			upgrade_avail = upgrade_avail
		})
	end
end



function action_flashops()
	local sys = require "luci.sys"
	local fs  = require "nixio.fs"

	local upgrade_avail = fs.access("/lib/upgrade/platform.sh")
	local reset_avail   = os.execute([[grep -E '"rootfs_data"|"ubi"' /proc/mtd >/dev/null 2>&1]]) == 0

	local restore_cmd = "tar -xzC/ >/dev/null 2>&1"
	local backup_cmd  = "sysupgrade --create-backup - 2>/dev/null"
	local image_tmp   = "/tmp/firmware.img"

	local function image_supported()
		return (os.execute("sysupgrade -T %q >/dev/null" % image_tmp) == 0)
	end

	local function image_checksum()
		return (luci.sys.exec("md5sum %q" % image_tmp):match("^([^%s]+)"))
	end

	local function storage_size()
		local size = 0
		if fs.access("/proc/mtd") then
			for l in io.lines("/proc/mtd") do
				local d, s, e, n = l:match('^([^%s]+)%s+([^%s]+)%s+([^%s]+)%s+"([^%s]+)"')
				if n == "linux" or n == "firmware" then
					size = tonumber(s, 16)
					break
				end
			end
		elseif fs.access("/proc/partitions") then
			for l in io.lines("/proc/partitions") do
				local x, y, b, n = l:match('^%s*(%d+)%s+(%d+)%s+([^%s]+)%s+([^%s]+)')
				if b and n and not n:match('[0-9]') then
					size = tonumber(b) * 1024
					break
				end
			end
		end
		return size
	end


	local fp
	luci.http.setfilehandler(
		function(meta, chunk, eof)
			if not fp then
				if meta and meta.name == "image" then
					fp = io.open(image_tmp, "w")
				else
					fp = io.popen(restore_cmd, "w")
				end
			end
			if chunk then
				fp:write(chunk)
			end
			if eof then
				fp:close()
			end
		end
	)

	if luci.http.formvalue("backup") then
		--
		-- Assemble file list, generate backup
		--
		local reader = ltn12_popen(backup_cmd)
		luci.http.header('Content-Disposition', 'attachment; filename="backup-%s-%s.tar.gz"' % {
			luci.sys.hostname(), os.date("%Y-%m-%d")})
		luci.http.prepare_content("application/x-targz")
		luci.ltn12.pump.all(reader, luci.http.write)
	elseif luci.http.formvalue("restore") then
		--
		-- Unpack received .tar.gz
		--
		local upload = luci.http.formvalue("archive")
		if upload and #upload > 0 then
			luci.template.render("admin_system/applyreboot")
			luci.sys.reboot()
		end
	elseif luci.http.formvalue("image") or luci.http.formvalue("step") then
		--
		-- Initiate firmware flash
		--
		local step = tonumber(luci.http.formvalue("step") or 1)
		if step == 1 then
			if image_supported() then
				luci.template.render("admin_system/upgrade", {
					checksum = image_checksum(),
					storage  = storage_size(),
					size     = (fs.stat(image_tmp, "size") or 0),
					keep     = (not not luci.http.formvalue("keep"))
				})
			else
				fs.unlink(image_tmp)
				luci.template.render("admin_system/flashops", {
					reset_avail   = reset_avail,
					upgrade_avail = upgrade_avail,
					image_invalid = true
				})
			end
		--
		-- Start sysupgrade flash
		--
		elseif step == 2 then
			local keep = (luci.http.formvalue("keep") == "1") and "" or "-n"
			luci.template.render("admin_system/applyreboot", {
				title = luci.i18n.translate("Flashing..."),
				msg   = luci.i18n.translate("The system is flashing now.<br /> DO NOT POWER OFF THE DEVICE!<br /> Wait a few minutes before you try to reconnect. It might be necessary to renew the address of your computer to reach the device again, depending on your settings."),
				addr  = (#keep > 0) and "192.168.2.1" or nil
			})
			fork_exec("sleep 1; killall dropbear uhttpd; sleep 1; /sbin/sysupgrade %s %q" %{ keep, image_tmp })
		end
	elseif reset_avail and luci.http.formvalue("reset") then
		--
		-- Reset system
		--
		luci.template.render("admin_system/applyreboot", {
			title = luci.i18n.translate("Erasing..."),
			msg   = luci.i18n.translate("The system is erasing the configuration partition now and will reboot itself when finished."),
			addr  = "192.168.2.1"
		})
		fork_exec("sleep 1; killall dropbear uhttpd; sleep 1; jffs2reset -y && reboot")
	else
		--
		-- Overview
		--
		luci.template.render("admin_system/flashops", {
			reset_avail   = reset_avail,
			upgrade_avail = upgrade_avail
		})
	end
end

function action_passwd()
	local p1 = luci.http.formvalue("pwd1")
	local p2 = luci.http.formvalue("pwd2")
	local stat = nil

	if p1 or p2 then
		if p1 == p2 then
			stat = luci.sys.user.setpasswd("root", p1)
		else
			stat = 10
		end
	end

	luci.template.render("admin_system/passwd", {stat=stat})
end

function action_reboot()
	local reboot = luci.http.formvalue("reboot")
	luci.template.render("admin_system/reboot", {reboot=reboot})
	if reboot then
		luci.sys.reboot()
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

function ltn12_popen(command)

	local fdi, fdo = nixio.pipe()
	local pid = nixio.fork()

	if pid > 0 then
		fdo:close()
		local close
		return function()
			local buffer = fdi:read(2048)
			local wpid, stat = nixio.waitpid(pid, "nohang")
			if not close and wpid and stat == "exited" then
				close = true
			end

			if buffer and #buffer > 0 then
				return buffer
			elseif close then
				fdi:close()
				return nil
			end
		end
	elseif pid == 0 then
		nixio.dup(fdo, nixio.stdout)
		fdi:close()
		fdo:close()
		nixio.exec("/bin/sh", "-c", command)
	end
end

function diag_command(cmd, addr)
	if addr and addr:match("^[a-zA-Z0-9%-%.:_]+$") then
		luci.http.prepare_content("text/plain")

		local util = io.popen(cmd % addr)
		if util then
			while true do
				local ln = util:read("*l")
				if not ln then break end
				luci.http.write(ln)
				luci.http.write("\n")
			end

			util:close()
		end

		return
	end

	luci.http.status(500, "Bad address")
end


function diag_commdom(cmd,addr)
--[[
	if addr and addr:match("^[a-zA-Z0-9%-%.:_]+$") then
		luci.http.prepare_content("text/plain")

		local util = io.popen(cmd % addr)
		if util then
			while true do
				local ln = util:read("*l")
				if not ln then break end
				luci.http.write(ln)
				luci.http.write("\n")
			end

			util:close()
		end

		return
	end
--]]
	if cmd == "at" then
		if addr then
			luci.http.prepare_content("text/plain")
			local util = io.popen(addr)
			if util then
				while true do
					local ln = util:read("*l")
					if not ln then break end
					luci.http.write(ln)
					luci.http.write("\n")
				end
				util:close()
			end
			return
		end
	else
		if addr then
			luci.http.prepare_content("text/plain")
			local util = io.popen(addr)
			if util then
				while true do
					local ln = util:read("*l")
					if not ln then break end
					luci.http.write(ln)
					luci.http.write("\n")
				end
				util:close()
			end
			return
		end
	end
	luci.http.status(500, "Bad address")
end

function split( str,reps )
    local resultStrList = {}
    string.gsub(str,'[^'..reps..']+',function ( w )
        table.insert(resultStrList,w)
    end)
    return resultStrList
end

function stchar(addr)
		return string.char(addr)
end

function diag_dom(addr)
	local stringaddr=""
	if string.match(addr,"&") then
		local addr1=split(addr,"&") --luci.sys.exec("echo "..addr.." | awk -F '&' '{print $2}'")
		for i = 1, #addr1 do  
			--print(addr1[i])
			--luci.sys.exec("echo ' '"..addr1[i].."' ' >> /tmp/addr1tmp")
			local status, err = pcall(stchar,addr1[i])
			
			if not status then
					stringaddr=stringaddr..addr1[i]
			else
				if tonumber(addr1[i]) then
					if tonumber(addr1[i]) <= 32 then
						stringaddr=stringaddr..addr1[i]
					else
						stringaddr=stringaddr..string.char(addr1[i])
					end
				else
					stringaddr=stringaddr..string.char(addr1[i])
				end
			end
		end 
		addr=stringaddr
	end
	diag_commdom("dom",addr)
end

function diag_at(addr)
--if string.find(addr,"&") ==1 then
	local stringaddr=""
	if string.match(addr,"&") then
		local addr1=split(addr,"&") --luci.sys.exec("echo "..addr.." | awk -F '&' '{print $2}'")
		for i = 1, #addr1 do  
			--print(addr1[i])
			--luci.sys.exec("echo ' '"..addr1[i].."' ' >> /tmp/addr1tmp")
			local status, err = pcall(stchar,addr1[i])
			
			if not status then
					stringaddr=stringaddr..addr1[i]
			else
				if tonumber(addr1[i]) then
					if tonumber(addr1[i]) <= 32 then
						stringaddr=stringaddr..addr1[i]
					else
						stringaddr=stringaddr..string.char(addr1[i])
					end
				else
					stringaddr=stringaddr..string.char(addr1[i])
				end
			end
		end 
		addr="sendat "..stringaddr..""
	else
		addr="sendat "..addr..""
	end
--else
--	addr="sendat "..addr..""
--end
	diag_commdom("at",addr)
end

function diag_ping(addr)
	diag_command("ping -c 5 -W 1 %q 2>&1", addr)
end

function diag_traceroute(addr)
	diag_command("traceroute -m 15 -w 1 %s 2>&1", addr)
end