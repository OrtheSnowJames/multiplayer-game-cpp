#!Bin/bash
#runs multiplayer-game-cpp + server
#how to use xterm: xterm -e "./app1; /bin/bash" & xterm -e "app2; /bin/bash" & ...
sudo nano environmentVars.sh
chmod +x environmentVars.sh
source ./environmentVars.sh
xterm -e "./server; /bin/bash" & xterm -e "./client; /bin/bash" 
