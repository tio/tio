local logins = {
    {
        serialnumber = "foo",
        username = "foouser",
        password = "foopass",
    },
    {
        serialnumber = "bar",
        username = "baruser",
        password = "barpass",
    },
    {
        serialnumber = "baz",
        username = "bazuser",
        password = "bazpass",
    },
}

for _, login in ipairs(logins) do
    send("\n")
    local found = expect(login.serialnumber .. ".*login:", 10)
    if (1 == found) then
        send(login.username .. "\n")
        expect("Password:")
        send(login.password .. "\n")
        break
    end
end
