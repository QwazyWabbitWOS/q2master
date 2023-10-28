			Quake 2 Master v1.2 by QwazyWabbit

This program is a modification of R1ch's GloomMaster server.
The GloomMaster only accepted heartbeats from Gloom servers but this version
will accept heartbeats from any Quake2 game server and respond to client
queries with the list of servers it knows about.

This version runs as a Windows Console application, a Windows Service
or a Linux console application or a Linux daemon. The daemon fork code
is based on BSD so it should also run on BSD or other Unix based systems
and even Mac OS/X without much modification.

This server is a single executable file, no other files are required.
In debug mode the server remains attached to the console that invoked it
and it prints status messages to the screen.

In Windows, it creates registry entries for the IP address and port that
it will bind to, this allows for binding to specific addresses and ports
on multi-homed network cards in service mode since services usually don't
process or receive command line arguments.

Windows Installation is simple.

1. Copy the program executable to a directory you intend to run it from.
   This is usually /windows/system32 for services but you can put it
   in a general tools folder if you prefer.

2. Start a command prompt as Administrator in the folder containing the 
   executable, type: "master -ip xxx.xxx.xxx.xxx" to set the IP address 
   to bind, together with any other necessary settings. This sets up the
   registry parameters in HKEY_LOCAL_MACHINE\SOFTWARE\Q2MasterServer for
   use by the Q2 Master service at startup. If you are running the master
   behind a NAT firewall with co-located game servers then use -wanip and
   -lanip switches to allow the master to translate the game server local
   addresses to the WAN side IP address.

3. Type "master -install" to install the service.

4. Type "net start q2masterserver" to start the service. The server listens
   on UDP port 27900 by default and this is where most clients expect
   Q2 master servers to be. The service looks up the registry keys defined
   by the commands in step 2 and uses them to configure the master.
   Once started, the service will autostart when Windows boots. 

Linux installation varies, so I won't attempt to describe it here. Starting
the process is simple, command line: "master -ip xxx.xxx.xxx.xxx" will cause
the server to bind to the specified IP and detatch from the console that
started the server. Use "master -debug -ip x.x.x.x" to keep it in debug mode.
Use whatever method your Linux distro requires to install it as a daemon.

// General command line switches:

// -debug	Asserts debug mode. The program prints status messages to console
//			while running. Shows pings, heartbeats, number of servers listed.

// -ip xxx.xxx.xxx.xxx causes server to bind to a particular IP address when
//	used with multi-homed hosts. Default is 0.0.0.0 (any).

// -lanip xxx.xxx.xxx.xxx specifies the current host IP address inside a LAN.
// -wanip xxx.xxx.xxx.xxx specifies the WAN IP address of a server behind a NAT firewall.
// These two switches are used to allow a master server to be co-located with
// LAN game servers. They cause the master server to translate the LAN IP to the
// specified WAN IP when sending the server list to clients.

// -port xxxxx causes server to bind to a particular port. Default is 27900.
// Default port for Quake 2 master servers is 27900. If you depart from this
// you need to communicate this to your users somehow. This feature is included
// since this code could be modified to provide master services for other games.

// *** Windows *** usage:

// Place executable in %systemroot%\system32 or other known location.
// To debug it as a console program, command: "master -debug" and it outputs
// status messages to the console. Ctrl-C shuts it down.
//
// From a cmd prompt type: q2master -install to install the service with defaults.
// The service will be installed as "Q2MasterServer" in the Windows service list.
//

// -install	Installs the service on Windows.
// -remove	Stops and removes the service.
// When the service is installed it is installed with "Automatic" startup type
// to allow it to start up when Windows starts. Use SCM to change this.

// Other commands:
// net start q2masterserver to start the service.
// net stop q2masterserver to stop the service.
//
// Use the Services control panel applet to start/stop, change startup modes etc.
// To uninstall the server type "master -remove" to stop and remove the active service.

// *** Linux *** usage:

// -debug Sets process to debug mode and it remains attached to console.
// If debug is not specified on command line the process forks a daemon and
// detaches from terminal.
//
// Send the process a SIGTERM to stop the daemon. "kill -n SIGTERM <pid>"
// Use "netstat -anup" to see the processes/pids listening on UDP ports.
// Use "ps -ux" to list detached processes, this will show the command line that
// invoked the q2master process.
// 
