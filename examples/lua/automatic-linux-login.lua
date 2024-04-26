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

local found, match_str = expect("\\w+- login:", 10)
if (1 == found) then
    local hostname = string.match(match_str, "^%w+")
    local login = logins[hostname]
    if (nil ~= login) then
        send(login.username .. "\n")
        expect("Password:")
        send(login.password .. "\n")
    else
        io.write("\r\nDon't know login info for " .. hostname .. "\r\n")
    end
else
    io.write("\r\nDidn't find a login prompt\r\n")
end
