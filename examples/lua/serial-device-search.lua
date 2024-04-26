io.write("Searching... ")

local device = tty_search()

io.write("done\r\n")

for i in ipairs(device) do
        io.write("\r\n" .. device[i]["path"] .. "\r\n")
        io.write(" tid = " .. device[i]["tid"] .. "\r\n")
        io.write(" uptime = " .. device[i]["uptime"] .. "\r\n")
        io.write(" driver = " .. device[i]["driver"] .. "\r\n")
        io.write(" description = " .. device[i]["description"] .. "\r\n")
end
