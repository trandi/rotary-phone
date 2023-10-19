Depends on (all of which I downloaded and built locally):
- [libunifex](https://github.com/facebookexperimental/libunifex)
- [fmtlib](https://github.com/fmtlib/fmt)
- [pigpio](https://abyz.me.uk/rpi/pigpio/)
- [asound](https://www.alsa-project.org/wiki/Main_Page) - simply installed with `sudo apt install libasound2-dev`

2 things that I'd like to improve by making more async: 
- the `Sounds::playTone()` which builds PCM values and send them to the sound card synchronously each time (I had to insert a few milliseconds gap after each iteration in order to give time to other tasks to run - luckily inaudiable to the human ear)
- `Subprocess::exec()` synchronously waits for the result from each command line

For both the above there has to be a better way. Maybe something along the lines of [`linux::io_uring_context`](https://github.com/facebookexperimental/libunifex/blob/main/doc/api_reference.md#linuxio_uring_context) ?

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

.

I had quite a lot of issues and spent literally weeks trying figure out why `Phone::ring()` wasn't working as it should: it would either hang forever or hit the bell continuously ignoring the **delay** in between.
It turns out the problem was in `Linphone::monitorIncomingCalls()` where I defined a **lambda** creating a sender and then took that lambda by **reference** into the lambda composing the final sender returned from the method. 
That capture (lambda or not)  would go out of scope by the time the returned sender was executed which explains why I'd have "random" results that didn't make any sense (behaviour would change depending on simply defining unused methods or not, something which only lately I realised was a clear sign of Undefined Behaviour).
Overall, somewhat simliar to why one should avoid [Capturing Lambda Coroutines](https://clang.llvm.org/extra/clang-tidy/checks/cppcoreguidelines/avoid-capturing-lambda-coroutines.html).
