# RPC++
RPC++ is a tool for Discord RPC (Rich Presence) to let your friends know about your Linux system

## Installing requirements
### Arch based systems
```sh
pacman -S unzip
```

## Building
**GNU Make**, and **Discord Game SDK** are **required**. To see more information about setting up Discord Game SDK, see [DISCORD.md](./DISCORD.md)

To build RPC++, use the command: 
```sh
make
```

## Installing & Running
To install RPC++, run the this command:
```sh
sudo make install
```
You can run the app from any directory with
```sh
rpcpp
```

To run manually (without installing) you need to start `./build/rpcpp` with the variables `LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$(pwd)/lib"`

Optionally for systemd users there is a user-side `rpcpp.service` in this repo that can be installed to `~/.local/share/systemd/user/`, started and enabled on boot using
```sh
systemctl --user enable --now rpcpp
```

## Features
- Displays your distro with an icon (supported: Arch)
- Displays the focused window's class name with an icon (see supported apps [here](./APPLICATIONS.md))
- Displays CPU and RAM usage %
- Displays your window manager (WM)
- Displays your uptime
- Refreshes every second
  
![Preview of the rich presence](./screenshot.png)

## Contributing
You can make pull requests, to improve the code or if you have new ideas, but I don't think I will update the code very often.

## Supporting
Want to support author? That's great! Joining my [discord server](https://grial.tech/discord) and subscribing to my [YouTube channel](https://www.youtube.com/channel/UCi-C-JNMVZNpX9kOs2ZLwxw) would help a lot!

Are you a rich boi? You can send author XMR through this address:
```
48DM6VYH72tRfsBHpLctkNN9KKPCwPM2gU5J4moraS1JHYwLQnS1heA4FHasqYMA66SVnusFFPb3GAyW5yBPBwLRAKJuvT1
```
