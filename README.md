# linux-configs

The starting point is your `home folder (~)`
- `v1` is for `sway` and `i3`
- `v2` is for `i3`
- `v3` is for `sway`, using `swaybar`
- `v4` is for `i3` and `sway`
- `config` file for `i3blocks` is `.config/i3blocks/config`

## Software needed for `i3`
- for Ubuntu LTS, use the repositories below:  
https://i3wm.org/docs/repositories.html
- a new version of `i3blocks` - compile it; version for Debian and Ubuntu is from 2015  
- `autorandr,` `redshift`, `dmenu`, `udiskie`, `hsetroot`, `kitty`, `picom`  
https://launchpad.net/~regolith-linux/+archive/ubuntu/release?field.series_filter=focal  

## Useful software
- nice mouse cursors  
https://gitlab.com/Enthymeme/hackneyed-x11-cursors
- `calcurse`, console calendar with todos, reminders  
https://www.calcurse.org/
- `hsetroot`, for changing wallpapers, works with `compton/picom`
## How to clone and commit to this repo
1. clone the repo
```
git clone https://github.com/georgeabr/linux-configs; cd linux-configs/
```

### To remember git credentials, to save username/password
```
git config credential.helper store
```

### To configure identity, required by git
```
git config --global user.email "your email"
git config --global user.name "georgeabr"
```

2. commit to this repo any modifications made

```
$ git add .; git commit -m "modified"; git push -u origin master
```

## Commands for i3/sway
```
- mod+w will put all the windows into a set of tabs
- mod+s will put all the windows into a stack
- mod+e will put everything back
```

## For sway
1. Alternative to `dmenu`
```
install bemenu
```
2. disable audio bell - the command below in `/etc/profile`  
```
$ xset -b
``` 
