tio.read(1000, 6000) -- initial config
tio.write("\n")
tio.msleep(100)
tio.read(650, 60) -- main menu
tio.write("S") -- S menu
tio.msleep(30)
tio.read(650, 60)
tio.write("t") -- Parallel Value Table
tio.read(650, 60)
while true do
  tio.msleep(1000)
  tio.write("t")
  tio.read(650, 50) -- repeat PVT forever
end
