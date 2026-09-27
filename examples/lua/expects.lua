--
-- example of intaction with AT modem.
--
tio.write("AT\r")
local matches, all = tio.expect("OK", 1000)
if matches == nil then
    tio.echo("no response 1\r\n")
    os.exit(0)
end
msleep(200)
tio.read(1000, tio.C.NOWAIT)
tio.write("ATFANTASYCMD\r")
local idx, matches, all = tio.expects({"OK", "ERROR", "BUSY"}, 1000)
if idx == nil then
    tio.echo("no response 2\r\n")
    os.exit(0)
end

-- this display 2, ERROR
print(idx, matches[1])
os.exit(0)
