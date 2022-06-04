<!--
SPDX-FileCopyrightText: Red Hat Inc
SPDX-License-Identifier: LGPL-2.1-or-later
-->

# The blksum demo

This document describes the complicated manual process for producing the
blksum demo.

## Recording

Assuming you have a demo directory with the required images, according
to the instructions in blksum.py, here is how you record a new demo.

1. Start a new terminal window in the demo directory
2. Set the windows size to 90x24
3. Start recording:

       asciinema rec blksum.cast

4. Run the demo:

       python blksum.py

5. Press `Ctrl+D` to end recording

6. Check the recording using:

       asciinema play blksum.cast

7. When recording is done, copy the file to the source and commit.

## Creating GIF image

1. Create a GIF image from cast file

   Currently using gifcast:

       https://dstein64.github.io/gifcast/

   Using these settings:

       size:            small
       contrast gain:   none
       cursor:          block
       font:            monospace
       theme:           none

2. Right click the image and select `Save as...`

## Optimizing the GIF

The generated GIF is way too large 15.8 MiB then it should be. We can
optimize it to 489 KiB by optimizing the GIF layers.

This is critical if you use https://imgur.com/ since it converts big
GIFs to video (mp4), which does not work well for embedding in the
`README.md` in GitHub and GitLab.

To optimize the GIF run:

    gifsicle --optimize=3 blksum-demo.gif -o blksum-demo-opt.gif

You can also open the file in `GIMP` and apply the `Fiter > Animation >
Optimize (for GIF)` filter.

## Publishing the image

Currently we are using https://imgur.com/, but this should work on every
web server.

1. Upload the image to imgur
2. Right click on the image and select `Copy image address`
3. Replace the address in the `README.md` link target
