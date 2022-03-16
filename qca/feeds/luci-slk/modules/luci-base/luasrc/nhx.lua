-- Licensed to the public under the Apache License 2.0.

local io   = require "io"
module "luci.nhx"

nhxver = ""
gitver = ""
nhxboard = "NHX"

function trim(str)
	return (str:gsub("^%s*(.-)%s*$", "%1"))
end

local fd = io.open("/rom/etc/openwrt_build", "r")

if fd then
	nhxver = trim(fd:read("*a"))
	fd:close()
end

fd = io.open("/rom/etc/nhx_git_version", "r")

if fd then
	gitver = trim(fd:read("*a"))
	fd:close()
end

nhxver = nhxver .. "(git-" .. gitver .. ")"

fd = io.open("/rom/etc/nhx_board", "r")

if fd then
	nhxboard = trim(fd:read("*a"))
	fd:close()
end
