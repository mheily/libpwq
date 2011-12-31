Set WshShell = WScript.CreateObject("WScript.Shell")
Set WshEnv = WshShell.Environment("SYSTEM")

WshEnv("MAKECONF_GUI") = "yes"
WshShell.Run "ruby configure", 7, FALSE
