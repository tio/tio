read(1000, 6000) -- initial config
send("\n")
msleep(100)
read(650, 60) -- main menu
send("S") -- S menu
msleep(30)
read(650, 60)
send("t") -- Parallel Value Table
read(650, 60)
while true do
  msleep(1000)
  send("t")
  read(650, 50) -- repeat PVT forever
end
