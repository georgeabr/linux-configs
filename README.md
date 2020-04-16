# linux-configs
.
The starting point is your `home folder (~)`
- `v1` is for `sway` and `i3`
- `v2` is for `i3`
- `config` file for `i3blocks` is `.config/i3blocks/config`

## Useful software

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
