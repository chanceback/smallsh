# smallsh (Custom Linux Shell)
## About
- Developed a bash-like shell with a command line interface
- Uses Linux Process API to create/coordiante/terminate local processes
- Implemented support for signal handling, variable expansion and input/output redirection
- Allows user commands to be run in foreground or background

## Screenshots
Parsing Comments and Variable Expansion:
![Parsing Comments and Variable Expansion](https://drive.google.com/uc?export=view&id=18066VNjv4hfPY8UR8kxDCCpbIEKFO9yD)

Folder Navigation and Built-in cd Command:
![cd cmd](https://drive.google.com/uc?export=view&id=1mdugGoGagGliqqfHnQldCDYZEUW-FFj4)

Input and Output Redirection:
![in out redirection](https://drive.google.com/uc?export=view&id=12XEr82JmJPxmCzmwwTovDAKYpGwGCiWy)

Running a Process In the Background:
![background processes](https://drive.google.com/uc?export=view&id=1v91RbvevubR_zy34O5lOLbnxUbqWbbz1)

Custom Signals Handling and Built-in Exit Command:
![exit and signals handling](https://drive.google.com/uc?export=view&id=1MRLWRhv3gI4jlaDlP2d5ZmaZR2CEpy3J)


## Usage Setup
If you'd like to run smallsh, download and unzip the github files. 

Next call `make` in the same directory. It uses the gcc compiler so make sure your linux distro has gcc installed.

Lastly run `PS1="$ " ./smallsh` and you shell should start up!
