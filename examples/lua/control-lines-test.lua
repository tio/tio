tio.set{DTR=high, RTS=low}
tio.msleep(100)
tio.set{DTR=low, RTS=high}
tio.msleep(100)
tio.set{RTS=toggle}
