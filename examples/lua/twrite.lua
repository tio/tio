--
-- tio.twrite() uses output-mapping, output-delay and input-mode.
--
-- Input-mode HEX is very slow, so it has limited use.
--
-- This script sends
--   Hello.
--   This is Hex mode.
--   Bye.
--

tio.set_input_mode(tio.C.IM_NORMAL)
tio.twrite("Hello.\r\n")

tio.set_input_mode(tio.C.IM_HEX)
tio.twrite("5468697320697320486578206d6f64652e0d0a") -- "This is Hex mode."

tio.set_input_mode(tio.C.IM_LINE)
tio.twrite("Bye.\r\n")

tio.set_input_mode(tio.C.IM_NORMAL)

