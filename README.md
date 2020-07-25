# Big.Little.Gain
A simple volume plugin that allows both coarse and fine-grained volume control.

This is a VST/VST3/AU plugin that provides two gain knobs, big (coarse) and little (fine).
The coarse gain knob is good for adjusting the volume of the track overall, while the fine
gain knob is useful for volume effects, where you only want to change the volume a few dB.

![Screenshot 1](./images/screenshot-1.png)

# Purpose
I wanted to make a neat volume effect, but all the compressors I have go from -70 dB to 30 dB,
or have equally large ranges. When I wanted a sine wave that went from +2 to -2 dB, it looked like a flat line.

Here's a demo of Big.Little.Gain where I have a sine wave automating the fine gain, and a
decrease on the coarse gain because the plugin is loud.

![Demo 1](./images/demo-1.gif)

The Fine Range slider controls the minimum and maximum values of Fine Gain, so you can get those
precise automation clips.