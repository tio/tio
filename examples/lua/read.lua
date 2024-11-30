read(1000, 6000) -- initial config
write("\n")
msleep(100)
read(650, 60) -- main menu
write("S") -- S menu
msleep(30)
read(650, 60)
write("t") -- Parallel Value Table
read(650, 60)
while true do
  msleep(1000)
  write("t")
  read(650, 50) -- repeat PVT forever
end
