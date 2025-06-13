local logins = {
    ["foo"] = {
        username = "foouser",
        password = "foopass",
    },
    ["bar"] = {
        username = "baruser",
        password = "barpass",
    },
    ["baz"] = {
        username = "bazuser",
        password = "bazpass",
    },
}

local hostname = tio.expect("^(%g+) login:", 10)
if hostname then
    local login = logins[hostname]
    if (nil ~= login) then
        tio.write(login.username .. "\n")
        tio.expect("Password:")
        tio.write(login.password .. "\n")
    else
        io.write("\r\nDon't know login info for " .. hostname .. "\r\n")
    end
else
    io.write("\r\nDidn't find a login prompt\r\n")
end
