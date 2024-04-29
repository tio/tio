set{DTR=high, RTS=low}
msleep(100)
set{DTR=low, RTS=high}
msleep(100)
set{RTS=toggle}
