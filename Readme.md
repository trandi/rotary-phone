Depends on (all of which I simply downloaded and built locally):
- [libunifex](https://github.com/facebookexperimental/libunifex)
- [fmtlib](https://github.com/fmtlib/fmt)


Linphone doesn't seem to be able to use other audio output than the default one.
In spite of chaning that default in `raspi-config`.
So I had to add `snd_bcm2835` to `/etc/modprobe.d/alsa-blacklist.conf` to completely disable loading the driver for the onboard headphone.

- installed `linphone-cli` and could use `linphonec` or `linphonecsh` to place calls
- however I had to also install `linphone` and start it once in UI mode (use `ssh -Y ...` and then start `linphone`) to generate `~/.config/linphone/linphonerc`
- *Only then* I was able to have `linphonecsh -init -c ~/.config/linphone/linphonerc` actually work and have it ringing

- `linphonecsh generic "answer"` to answer
- `linphonecsh generic "terminate"` to end call"
- `linphonecsh status hook`
  - `hook=on-hook`
  - `Incoming call from sip:red-phone@sip.linphone.org`
