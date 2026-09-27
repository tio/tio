# Interactive Lua Scripting

## Introduction

Tio provides an **interactive Lua scripting interface** that allows scripts and Lua commands to be executed while Tio is running.

In earlier versions, the script interpreter was restarted for each script execution.
The interpreter now **remains active**, preserving its state across executions.

This makes it possible to:

* run Lua commands interactively
* reuse functions defined in previously loaded scripts
* experiment with Lua code without restarting the interpreter
* avoid repeated initialization overhead when working with large scripts

---

## Starting Interactive Script Mode

To start interactive script mode, press:

**Ctrl-t r**

Tio displays the script prompt:

```
[08:45:46.514] Run Lua script
[08:45:46.514] Enter file name or "!" Lua commands or "@" interpreter directive:
>>
```

At this prompt you can:

| Input        | Description                              |
| ------------ | ---------------------------------------- |
| `filename`   | Execute a Lua script file                |
| `!commands`  | Execute Lua commands                     |
| `@directive` | Execute an interpreter control directive |

You can use the cursor keys and backspace key to edit the line and recall command history.
The history is maintained while tio is running.

---

## Running a Script File

To execute a Lua script file, enter the filename at the prompt.

Example script (`banner.tio`):

```lua
function tio.banner()
    local banner = [[
 _    _               _                _               __
| |_ (_)  ___    ___ | |_  __ _  _ __ | |_      _      \ \
| __|| | / _ \  / __|| __|/ _` || '__|| __|    (_)_____ | |
| |_ | || (_) | \__ \| |_| (_| || |   | |_  _   _|_____|| |
 \__||_| \___/  |___/ \__|\__,_||_|    \__|(_) (_)      | |
                                                       /_/
]]
    tio.echo(string.gsub(banner, "\n", "\r\n"))
end

tio.banner()
```

Run the script:

```
>> banner.tio
[09:08:13.786] Running script banner.tio
 _    _               _                _               __
| |_ (_)  ___    ___ | |_  __ _  _ __ | |_      _      \ \
| __|| | / _ \  / __|| __|/ _` || '__|| __|    (_)_____ | |
| |_ | || (_) | \__ \| |_| (_| || |   | |_  _   _|_____|| |
 \__||_| \___/  |___/ \__|\__,_||_|    \__|(_) (_)      | |
                                                       /_/
```

The script is loaded and executed immediately.
Any functions defined in the script remain available in the interpreter.

---

## Executing Lua Commands

Lua commands can be executed directly by prefixing them with `!`.

This allows you to interact with previously defined functions or variables.

Example script (`calc_sum.tio`):

```lua
function tio.calc_sum(t)
    local sum = 0
    for _, v in ipairs(t) do
        sum = sum + v
    end
    return sum
end

function tio.disp_sum(t)
    local sum = tio.calc_sum(t)
    tio.echo(sum)
    tio.echo("\r\n")
end
```

Example session:

```
>> calc_sum.tio
[09:15:22.372] Running script calc_sum.tio

<<press Ctrl-t r again>>
>> !t = {1,2,3,4,5,6,7,8,9,10}

<<press Ctrl-t r again>>
>> !tio.disp_sum(t)
55
>> !sum = tio.calc_sum(t); tio.echo(sum); tio.echo("\r\n")
55
```

---

## Interpreter Directives

Interpreter behavior can be controlled using directives prefixed with `@`.

### `@new`

Restart the script interpreter.

All interpreter state is cleared and the interpreter is reinitialized.

If a `script-init-file` is configured, it is executed again.

---

### `@doopt`

Execute the script specified by the `script` or `script-file` option, if present.

---

### `@nuldo=opt`

If **Ctrl-t r** is pressed and **Enter** is pressed without entering a command, `@doopt` is executed.

This is the default behavior for compatibility with Tio v3.9.

---

### `@nuldo=none`

If **Ctrl-t r** is pressed and **Enter** is pressed without entering a command, no action is performed.

This helps prevent accidental script execution.

---

### `@repl`

Start **REPL (Read–Eval–Print Loop)** mode.

---

## REPL Mode

Enter:

```
@repl
```

to start REPL mode.

In REPL mode:

* Lua commands can be entered continuously
* each line is executed immediately
* multi-line statements can be continued using a trailing `\`
* `@exit` leaves REPL mode

Example:

```
[10:17:30.215] Run Lua script
[10:17:30.215] Enter file name or "!" Lua commands or "@" interpreter directive:
>> @repl
[10:17:31.956] Enter Lua REPL mode (@exit to exit)
-> p=1
-> for i=1,10 do\
->     p = p * i\
-> end
-> print(p, "\r")
3628800
-> @exit
```

---

## Summary

The interactive Lua scripting interface provides:

* persistent interpreter state
* interactive Lua command execution
* script reuse without interpreter restart
* an integrated REPL environment

These capabilities make it easier to develop, test, and experiment with Tio scripts while the program is running.

Enjoy using Tio's Interactive Lua Scripting.
