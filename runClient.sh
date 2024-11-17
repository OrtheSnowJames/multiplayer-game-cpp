#runs multiplayer-game-cpp
sudo nano environmentVars.sh
chmod +x environmentVars.sh
source ./environmentVars.sh
chmod +x saveData.sh
source ./saveData.sh
xterm -e "./client; /bin/bash" 