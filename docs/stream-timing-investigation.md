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

## Required live validation

This is a candidate fix for the reported symptom, not a confirmed diagnosis
of this machine. Run the portable test build for at least 30 minutes with the
same console, network, decoder and quality settings. Record whether the
slowdown returns, and capture packet loss, queue depth and pending-frame age
from the statistics overlay before and during any slowdown.

If the timing patch does not resolve it, compare the default rendering preset
and a different supported hardware decoder separately, while monitoring GPU
load, memory and Wi-Fi latency. Those experiments distinguish GPU/driver
pressure and transport stalls from timestamp drift. No machine settings or
installed application files were changed as part of this investigation.
