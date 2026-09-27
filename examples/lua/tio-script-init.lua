--
-- tio's script-init file
--
-- Please add following to your tio config file.
--
-- script-init-file = /<<your-full-path>>/.tio-scipt-init.lua
--
-- note: tio module is already loaded.
--

function tio.conv_lf_to_crlf(str)
    local new_str = str:gsub("\n", "\r\n")
    return new_str
end

function tio.lprint(...)
    local args = {...}
    local argN = #args
    for i = 1, argN - 1 do
        io.write(tio.conv_lf_to_crlf(tostring(args[i])))
        io.write("\t")
    end
    if argN > 0 then
        io.write(tio.conv_lf_to_crlf(tostring(args[argN])))
    end
    io.write("\r\n")
end

function tio.banner()
    local banner = [[
 _    _               _                _               __
| |_ (_)  ___    ___ | |_  __ _  _ __ | |_      _      \ \
| __|| | / _ \  / __|| __|/ _` || '__|| __|    (_)_____ | |
| |_ | || (_) | \__ \| |_| (_| || |   | |_  _   _|_____|| |
 \__||_| \___/  |___/ \__|\__,_||_|    \__|(_) (_)      | |
                                                       /_/]]

    tio.lprint(banner)
end

tio.banner()
