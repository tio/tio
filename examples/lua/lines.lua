read(1000, 8000) -- read initial config
send("\n")
read(650, 100) -- main menu
send("S") -- S menu
n = 1
while n > 0 do -- while not empty, read more
  n, str = read_line(25)
end
while true do
  send("t") -- query PV table
  msleep(880)
  n = 1
  while n > 0 do -- while not empty, read more
    n, str = read_line(60)
    msleep(60)
  end
end
