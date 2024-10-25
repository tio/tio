send("\n")
msleep(100)
-- read buffer with 0.5 sec delay
read(160, 500)
send("S")
msleep(100)
-- read buffer
read(160, 500)
-- query Parallel Value Table
send("t")
print(read(300, 500))
while true do
  sleep(1)
  send("t")
  print(read(300, 500))
end
