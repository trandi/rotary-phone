Depends on (all of which I simply downloaded and built locally):
- [libunifex](https://github.com/facebookexperimental/libunifex)
- [fmtlib](https://github.com/fmtlib/fmt)


Linphone doesn't seem to be able to use other audio output than the default one.
In spite of chaning that default in `raspi-config`.
So I had to add `snd_bcm2835` to `/etc/modprobe.d/alsa-blacklist.conf` to completely disable loading the driver for the onboard headphone.
