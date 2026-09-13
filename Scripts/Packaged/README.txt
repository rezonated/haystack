Haystack - a needle in 4 million pieces of hay

Windows 10/11 x64 build, Shipping configuration, Unreal Engine 5.8.2.
Needs a DirectX 12 GPU with Shader Model 6 (Nanite, Lumen and virtual shadow maps). Target: RTX 5060 class at 1080p, High scalability.


HOW TO START
------------
Haystack.exe -> Single player. One pile, one needle, no network.

PlayLocal.bat -> Two instances on this machine, side by side in 960x540 windows: a listen server on the left and a client on the right that joins it. Both see the same pile, pieces one player moves show up for the other.

Host.bat -> A listen server for other machines on the LAN. Give players your IPv4 address. UDP port 7777 has to be reachable (Windows Firewall asks on first launch).

Join.bat <ip> -> Joins the listen server at <ip>, for example: Join.bat 192.168.1.20. Without an address it joins a server on this machine.

Every .bat accepts extra arguments and passes them to the game, for example -windowed -ResX=1920 -ResY=1080 or -log for a log window.


HOW TO PLAY
-----------
W A S D -> Move
Mouse -> Look
Space -> Jump
Left mouse button -> Pick up the piece under the crosshair, press again to throw it. The crosshair turns green over a piece within reach, the prompt names the key.

The needle is one piece of the pile, at least 40 cm below the surface and never higher than 2 m above the ground. Dig toward it by picking pieces away. Whoever lifts it wins, every player sees who found it.


CONSOLE
-------
The console is enabled in this Shipping build.

Hay.ToggleVisibility -> Hides or shows all hay. The needle stays visible, so you can confirm it exists.
Hay.NetStats <seconds> -> Logs connections, bandwidth and game thread time every N seconds. 0 stops.
Hay.DigBot [on|off|needle] -> A bot that digs on your player: random spots, or straight at the needle.
stat unit, stat fps -> The usual engine stats.


LOGS
----
%LOCALAPPDATA%\Haystack\Saved\Logs\Haystack.log, a second instance on the same machine writes Haystack_2.log.
