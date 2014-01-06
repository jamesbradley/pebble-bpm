pebble-bpm
=========
BPM is a Pebble app to get the Beats Per Minute of a piece of music.  Useful for DJs, musicians, music lovers etc.

Features
--------
- Single Mode - Get a BPM reading for a track
- Dual Mode - Get a second BPM reading and see the percentage offset between the two
- Watch vibrates to the beat
- Watch flashes the screen on each beat (inverts)
- Watch flashes the backlight on each beat (off by default)
- Tap Mode - Tap the watchface instead of the buttons (currently not working - see Bugs)

Instructions
------------
- Tap the top or bottom button in time with the music to see it's BPM
- Tap the other button (bottom or top) button to get a second BPM reading and see the offset between the two
- Tap the middle button to stop all timers
- Hold the middle button to see the settings

Bugs & Improvements
-------------------

- A slight rounding error with the calculation of decimal places
- A small issue with a inverter layer not hiding occasionally when stopping Dual Mode
- Pebble SDK Beta 4 bug - Persistent storage stops working after a while, meaning settings reset/wont save
- Pebble SDK Beta 4 limitation - tap detection is sometimes limited, and will only work with taps of 1.5 seconds or larger apart
- Potential improvement - Add a launch screen (first time only) to tell users which buttons do what, as I want people to be able to find the settings
- Potential improvement - Square off the corners of the ActivityBar to made it look less odd in Dual Mode
