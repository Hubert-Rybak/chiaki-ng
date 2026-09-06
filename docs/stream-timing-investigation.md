# Long-session streaming slowdown investigation

## Evidence and limits

The reported Windows symptom is normal playback initially, lag after roughly
15 minutes, and normal playback again after closing Chiaki and reconnecting.
The installed executable reports 1.10.0. Session logs show Vulkan hardware
decoding on AMD Radeon Graphics. The machine uses an RZ616 Wi-Fi adapter and
the high-quality libplacebo preset at 1080p/60.

Two September 6 sessions lasted approximately 11 and 14 minutes. They contain
bursts of missing video units and failed FEC earlier in the sessions, but no
corresponding logged decoder errors near their ends. These logs do not include
continuous render timings, GPU temperatures, memory usage, or presentation
clock error. They cannot establish the cause of the reported slowdown.

## Defect addressed

The decoder adds a rounded estimated duration to every synthetic packet PTS.
Its rate estimator ignores differences below 20 percent, so a small difference
between source cadence and local playback clock accumulates indefinitely.
For example, 59.94 fps input with a 16,667 microsecond increment falls about
0.88 seconds behind elapsed time after 15 minutes. Rounding alone at exactly
60 fps advances timestamps by 72 milliseconds over an hour. Resetting a
session resets the accumulated error.

The patch retains the existing duration estimator and lost-frame advancement,
but bounds synthetic timestamps to within two nominal frames of elapsed
monotonic sample-arrival time. It preserves timestamp ordering when delayed
frames arrive together. Ordinary arrival jitter within that tolerance does
not change nominal frame pacing. The first packet remains timestamp zero.

Regression tests simulate an hour at 60, 59.94, 60.06, 30 and 29.97 fps,
arrival jitter, missing frames, a five-second interruption, a packet burst,
and transitions between 60 and 30 fps. They execute the production timing
helper without requiring a console or real-time waits.

## Validation

The Windows portable build from commit
`bcc8538a658d07649178f98ff6bf7d61bc53c652` passed the unit suite, including
the timing regressions. The Linux unit suite also passed. The downloaded
Windows artifact matched its published SHA-256 digest, and its help command
exited successfully on the affected machine.

- [Windows build and tests](https://github.com/Hubert-Rybak/chiaki-ng/actions/runs/34044526371)
- [Linux tests](https://github.com/Hubert-Rybak/chiaki-ng/actions/runs/34044526380)

The user subsequently tested the portable build and reported that it appears
to resolve the slowdown, then requested finalization. The test duration was
not specified. This supports the fix for the reported symptom but does not
establish timestamp drift as the sole cause: the build also includes upstream
changes since the originally inspected 1.10.0 checkout.

If the slowdown recurs, capture packet loss, queue depth and pending-frame age
before and during it, along with GPU load, memory and Wi-Fi latency. No machine
settings or installed application files were changed during this work.
