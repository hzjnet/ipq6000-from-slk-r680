require("luci.sys")
require("luci.sys.zoneinfo")
require("luci.tools.webadmin")
require("luci.fs")
require("luci.config")
require "luci.model.uci"

local m, s, o

m = Map("apnset", translate("SIM Card Info"))
m:chain("luci")

s = m:section(TypedSection, "apnset", translate("SIM Card Info"))
s.anonymous = true
s.addremove = false


-- config for sim card

require "nixio"
local sys = require "luci.sys"
luci.util   = require "luci.util"
luci.fs     = require "luci.fs"
luci.ip     = require "luci.ip"

function getUrlFileName( strurl, strchar, bafter)
    local ts = string.reverse(strurl)  
    local param1, param2 = string.find(ts, strchar)
    local m = string.len(strurl) - param2 + 1
    local result  
    if (bafter == true) then  
        result = string.sub(strurl, m+1, string.len(strurl))
    else
        result = string.sub(strurl, 1, m-1)
    end
    return result
end


d = s:option( DummyValue, "_detected")
d.rawhtml = true
d.cfgvalue = function(s)
	local list = io.popen("cat /tmp/lte/data2")

	if list then
		local files = { "<table width=\"100%\" cellspacing=\"10\">" }

		while true do
			local ln = list:read("*l")
			if not ln then
				break
			else
				files[#files+1] = "<tr><td width=\"30%\">"

					local strlen = string.len(ln)
					if strlen == 0 then
						files[#files+1] = luci.util.pcdata(ln)
						files[#files+1] = "</td><td>"
					else
						if string.find(ln, ":") == nil then
							files[#files+1] = luci.util.pcdata(ln)
							files[#files+1] = "</td><td>"
						else
							local result_head = getUrlFileName(ln, ":", false)
							local result_head_new = translate(result_head)
							files[#files+1] = luci.util.pcdata(result_head_new)
							files[#files+1] = "</td><td>"
							local result_content = getUrlFileName(ln, ":", true)
							local result_content_new = translate(result_content)
							--local str = table.concat({result_head_new,":",result_content_new})

							files[#files+1] = luci.util.pcdata(result_content_new)
							files[#files+1] = "</td><td>"
						end
					end
			end
		end

		list:close()
		files[#files+1] = "</table>"

		return table.concat(files, "")
	end

	return "<em>" .. translate("No files found") .. "</em>"
end

return m

