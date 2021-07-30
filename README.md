Author(s): gfernan2, Harrison Snow

### Files in Directory:

The following files and directories should be present in the directory:

	- README
	- Makefile
	- netpong.cpp

### Compilation

Compilation is handled through the Makefile. The command 'make' will compile the code
netpong.cpp into the executable named 'netpong'. 

### Usage

The usage of netpong is as follows:

To run the game as a host:

	./netpong --host [PORT]

where PORT is the intended port to be hosted by the server. The host will then be asked to
enter a difficulty (easy, medium, hard) and the number of rounds to play.

To run the game as a client:

	./netpong [HOST] [PORT]

where HOST and PORT are where the host is hosting the game.

To control the game as host, control the paddle with the W and S keys.
To control the game as client, control the paddle with the up and down arrow keys.

As defined in the specifications, a user needs to score 2 points per round. The game automatically terminates (gracefully)
if the specified number of rounds played is reached. Otherwise, if the user terminates the program with Ctrl+C, the program
should (kind of) terminate gracefully

### Notes

- Average error checking as usual. Please be gentle.

- You may notice that when a user scores a point, the score on the opponent's side is not updated immediately, but instead after the countdown.
The scores nevertheless sync properly over the course of normal gameplay. 

- We were unable to satisfy the condition that when one client disconnects, the other client is terminated. For some reason the message sent
in the interrupt handler is not reached by the other.
