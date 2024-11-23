#put apt packages here
read -p "apt or pacman" packagemanager
if [packagemanager == "apt"]; then
sudo apt install build-essential git
sudo apt install libboost-all-dev
sudo apt install nlohmann-json3-dev
sudo apt install libraylib-dev
sudo apt install xterm
fi
else then
fi
#put git packages here
cd /home/
git clone https://github.com/raysan5/raylib
#put code packages here
code --install-extension ms-vscode.cpptools
