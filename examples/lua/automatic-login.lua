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

send("\n")
local found, match_str = expect("\\w+- login:", 10)
if (1 == found) then
    local model = string.match(match_str, "^%w+")
    local login = logins[model]
    if (nil ~= login) then
        send(login.username .. "\n")
        expect("Password:")
        send(login.password .. "\n")
    else
        print("\r\nDon't know login info for " .. model .. "\r\n")
    end
else
    print("\r\nDidn't find a login prompt\r\n")
end
