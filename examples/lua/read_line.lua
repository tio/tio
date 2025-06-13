tio.read(1000, 8000) -- read initial config
tio.write("\n")
tio.read(650, 100) -- main menu
tio.write("S") -- S menu
repeat
  str = tio.readline(25)
until str == nil
while true do
  tio.write("t") -- query PV table
  tio.msleep(880)
  repeat
    str = tio.readline(60)
    tio.msleep(60)
  until str == nil
end
