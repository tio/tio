send("\n")
msleep(100)
read_ts(650, 60) -- main menu
send("S") -- S menu
msleep(30)
read_ts(650, 60)
send("t") -- Parallel Value Table
read_ts(650, 60)
while true do
  msleep(970)
  send("t")
  read_ts(650, 50) -- repeat PVT forever
end
